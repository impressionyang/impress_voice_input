#include "voice_input_service.h"
#include "audio/audio_capture.h"
#include "sense_voice_engine.h"
#include "app/config_manager.h"
#include "utils/logger.h"

// 平台特定的快捷键和文本注入
#ifdef PLATFORM_WINDOWS
#include "win_hotkey.h"
#include "win_text_injector.h"
#elif defined(PLATFORM_MACOS)
#include "mac_hotkey.h"
#include "mac_text_injector.h"
#else
#include "caps_lock_voice_hotkey.h"
#include "wayland_text_injector.h"
#endif

#include <QThread>
#include <QTimer>
#include <QtConcurrent>

static const char* const kTag = "VoiceInputService";

namespace impress {

struct VoiceInputService::Impl {
    AudioCapture* audioCapture = nullptr;
    SenseVoiceEngine* sttEngine = nullptr;
    CapsLockVoiceHotkey* hotkey = nullptr;
    WaylandTextInjector* injector = nullptr;
};

VoiceInputService::VoiceInputService(ConfigManager* configManager,
                                     SenseVoiceEngine* sttEngine,
                                     QObject* parent)
    : QObject(parent)
    , configManager_(configManager)
    , impl_(std::make_unique<Impl>())
{
    impl_->sttEngine = sttEngine;

    // 长按确认定时器（1s）
    longPressTimer_ = new QTimer(this);
    longPressTimer_->setSingleShot(true);
    connect(longPressTimer_, &QTimer::timeout, this, [this]() {
        if (state_ == PreRecording) {
            state_ = Recording;
            emit statusChanged("正在录音...");
            LOG_DEBUG(kTag, "状态转换: PreRecording → Recording (长按确认)");
        }
    });

    // 松开后冷却定时器
    cooldownTimer_ = new QTimer(this);
    cooldownTimer_->setSingleShot(true);
    connect(cooldownTimer_, &QTimer::timeout, this, [this]() {
        if (state_ == Cooldown) {
            state_ = Idle;
            LOG_DEBUG(kTag, "状态转换: Cooldown → Idle (冷却结束)");
        }
    });
}

VoiceInputService::~VoiceInputService() {
    stop();
}

bool VoiceInputService::start() {
    if (running_) return true;

    // 1. 初始化音频采集
    impl_->audioCapture = new AudioCapture(this);
    connect(impl_->audioCapture, &AudioCapture::audioDataReady,
            this, &VoiceInputService::onAudioData);

    // 2. STT 引擎已作为参数传入（共享全局实例）

    // 3. 初始化全局快捷键
    impl_->hotkey = new CapsLockVoiceHotkey(this);
    connect(impl_->hotkey, &CapsLockVoiceHotkey::recordingStarted,
            this, &VoiceInputService::onHotkeyActivated);
    connect(impl_->hotkey, &CapsLockVoiceHotkey::recordingStopped,
            this, &VoiceInputService::onHotkeyDeactivated);
    connect(impl_->hotkey, &CapsLockVoiceHotkey::ready,
            this, [this]() {
                emit statusChanged("语音输入就绪（快捷键已注册）");
            });
    connect(impl_->hotkey, &CapsLockVoiceHotkey::error,
            this, &VoiceInputService::error);

    // 4. 初始化文本注入器
    impl_->injector = new WaylandTextInjector(this);
    if (!impl_->injector->initialize()) {
        emit error("文本注入器初始化失败，无法注入识别结果");
        LOG_ERROR(kTag, "文本注入器初始化失败");
    }

    // 启动快捷键（首次会弹出授权对话框）
    if (!impl_->hotkey->start()) {
        emit error("全局快捷键启动失败");
        return false;
    }

    running_ = true;
    state_ = Idle;
    emit statusChanged("语音输入已启动（等待授权...）");
    LOG_INFO(kTag, "语音输入服务已启动");
    return true;
}

void VoiceInputService::stop() {
    if (!running_) return;

    longPressTimer_->stop();
    cooldownTimer_->stop();

    if (impl_->audioCapture) {
        impl_->audioCapture->stop();
    }
    if (impl_->hotkey) {
        impl_->hotkey->stop();
    }

    running_ = false;
    recording_ = false;
    state_ = Idle;
    audioBuffer_.clear();

    LOG_INFO(kTag, "语音输入服务已停止");
}

void VoiceInputService::onHotkeyActivated() {
    // Recording 状态：屏蔽所有重复 Activated（防抖核心）
    if (state_ == Recording) {
        LOG_DEBUG(kTag, "忽略 Activated (Recording 状态屏蔽)");
        return;
    }

    // Cooldown 状态：冷却期内忽略
    if (state_ == Cooldown) {
        LOG_DEBUG(kTag, "忽略 Activated (冷却期内)");
        return;
    }

    // Idle 状态 → 开始预录音
    if (state_ == Idle) {
        state_ = PreRecording;
        recording_ = true;
        audioBuffer_.clear();

        longPressTimer_->start(longPressThreshold_);

        int deviceIndex = configManager_->get("audio.input_device").toInt();
        int sampleRate = configManager_->get("stt.sample_rate").toInt();
        int bufferSizeMs = configManager_->get("audio.buffer_size_ms").toInt();
        impl_->audioCapture->start(deviceIndex, sampleRate, bufferSizeMs);

        LOG_DEBUG(kTag, "状态转换: Idle → PreRecording");
        emit statusChanged("等待长按确认...");
        return;
    }

    // PreRecording 状态收到重复 Activated → 忽略（防抖）
    if (state_ == PreRecording) {
        LOG_DEBUG(kTag, "忽略重复 Activated (PreRecording 防抖)");
        return;
    }
}

void VoiceInputService::onHotkeyDeactivated() {
    LOG_DEBUG(kTag, QString("Deactivated (state=%1)").arg(recording_ ? "recording" : "idle"));

    // Cooldown 状态的 Deactivated → 忽略
    if (state_ == Cooldown) {
        LOG_DEBUG(kTag, "忽略 Deactivated (Cooldown 状态屏蔽)");
        return;
    }

    recording_ = false;
    longPressTimer_->stop();

    // 停止音频采集
    if (impl_->audioCapture && impl_->audioCapture->isRunning()) {
        impl_->audioCapture->stop();
    }

    if (state_ == PreRecording) {
        // 短按 → 模拟 CapsLock 切换大小写
        state_ = Idle;
        LOG_DEBUG(kTag, "短按，模拟 CapsLock");
        simulateCapsLock();
        emit statusChanged("短按：切换 CapsLock");
    } else if (state_ == Recording) {
        // 长按后松开 → 停止录音并转写
        state_ = Idle;
        LOG_DEBUG(kTag, "状态转换: Recording → Idle (松开转写)");
        stopRecordingAndTranscribe();
    }

    // 启动冷却期
    state_ = Cooldown;
    cooldownTimer_->start(releaseCooldownMs_);
    LOG_DEBUG(kTag, QString("状态转换: → Cooldown (%1ms)").arg(releaseCooldownMs_));
}

void VoiceInputService::onAudioData(const std::vector<float>& samples, int sampleRate) {
    if (!recording_) return;

    audioSampleRate_ = sampleRate;
    audioBuffer_.insert(audioBuffer_.end(), samples.begin(), samples.end());
}

void VoiceInputService::stopRecordingAndTranscribe() {
    if (audioBuffer_.empty()) {
        emit statusChanged("未检测到音频输入");
        return;
    }

    emit statusChanged("正在识别...");

    QString language = configManager_->get("stt.language").toString();

    (void)QtConcurrent::run([this, buffer = audioBuffer_, lang = language]() {
        QString text;

        if (!impl_->sttEngine->isLoaded()) {
            LOG_WARNING(kTag, "模型未加载，跳过推理");
            text = "[错误] 模型未加载，请先在配置中设置模型路径";
        } else {
            auto result = impl_->sttEngine->infer(buffer, audioSampleRate_, lang);
            text = result.text;
        }

        QMetaObject::invokeMethod(this, [this, text]() {
            onRecognitionComplete(text);
        }, Qt::QueuedConnection);
    });

    audioBuffer_.clear();
}

void VoiceInputService::onRecognitionComplete(const QString& text) {
    if (text.isEmpty()) {
        emit statusChanged("识别结果：无语音输入");
        return;
    }

    emit recognitionResult(text);
    emit statusChanged(QString("识别结果: %1").arg(text));

    // 注入文本到光标位置
    if (impl_->injector && impl_->injector->isInitialized()) {
        impl_->injector->injectText(text);
        LOG_INFO(kTag, QString("文本已注入: %1").arg(text));
    } else {
        LOG_WARNING(kTag, "文本注入器未就绪，无法注入");
    }
}

void VoiceInputService::simulateCapsLock() {
    if (impl_->injector && impl_->injector->isInitialized()) {
        impl_->injector->simulateKeysym(0xffe5);
        LOG_DEBUG(kTag, "模拟 CapsLock 按键已注入");
    } else {
        LOG_WARNING(kTag, "文本注入器未初始化，无法模拟 CapsLock");
    }
}

} // namespace impress
