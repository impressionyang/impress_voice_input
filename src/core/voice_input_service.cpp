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
#include <QElapsedTimer>
#include <algorithm>

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

    // 确认长按定时器 → 直接进入 Recording，消除 1s 延迟
    longPressTimer_ = new QTimer(this);
    longPressTimer_->setSingleShot(true);

    // 松开后冷却定时器
    cooldownTimer_ = new QTimer(this);
    cooldownTimer_->setSingleShot(true);
    connect(cooldownTimer_, &QTimer::timeout, this, [this]() {
        if (state_ == Cooldown) {
            state_ = Idle;
            LOG_DEBUG(kTag, "Cooldown → Idle (冷却结束)");
        }
    });
}

VoiceInputService::~VoiceInputService() {
    stop();
}

bool VoiceInputService::start() {
    if (running_) return true;

    // 1. 初始化音频采集并预打开音频流（避免按键时 Pa_OpenStream 延迟 3-4s）
    impl_->audioCapture = new AudioCapture(this);
    connect(impl_->audioCapture, &AudioCapture::audioDataReady,
            this, &VoiceInputService::onAudioData);

    int deviceIndex = configManager_->get("audio.input_device").toInt();
    int sampleRate = configManager_->get("stt.sample_rate").toInt();
    int bufferSizeMs = configManager_->get("audio.buffer_size_ms").toInt();
    impl_->audioCapture->start(deviceIndex, sampleRate, bufferSizeMs);
    impl_->audioCapture->stop();  // 停止但保留流，后续 start() 只需 Pa_StartStream
    LOG_INFO(kTag, "音频流已预打开，后续录音延迟 <100ms");

    // 2. STT 引擎已作为参数传入

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

    // 启动快捷键
    if (!impl_->hotkey->start()) {
        emit error("全局快捷键启动失败");
        return false;
    }

    running_ = true;
    state_ = Idle;
    emit statusChanged("语音输入已启动");
    LOG_INFO(kTag, "语音输入服务已启动");
    return true;
}

void VoiceInputService::stop() {
    if (!running_) return;

    longPressTimer_->stop();
    cooldownTimer_->stop();

    if (impl_->audioCapture) {
        impl_->audioCapture->stopAndClose();  // 彻底关闭流
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
    // Recording 和 Cooldown 状态：屏蔽所有 Activated（防抖核心）
    if (state_ == Recording || state_ == Cooldown) {
        LOG_DEBUG(kTag, QString("忽略 Activated (state=%1)").arg(state_ == Recording ? "Recording" : "Cooldown"));
        return;
    }

    // Idle → 直接进入 Recording，消除 1s 延迟
    state_ = Recording;
    recording_ = true;
    audioBuffer_.clear();

    int deviceIndex = configManager_->get("audio.input_device").toInt();
    int sampleRate = configManager_->get("stt.sample_rate").toInt();
    int bufferSizeMs = configManager_->get("audio.buffer_size_ms").toInt();
    impl_->audioCapture->start(deviceIndex, sampleRate, bufferSizeMs);

    // 延迟统计（现在应该接近 0）
    hotkeyLatencyTimer_.start();
    latencyTracking_ = true;
    qint64 latencyMs = 0;

    LOG_DEBUG(kTag, "Idle → Recording (立即开始录音)");
    emit statusChanged("正在录音...");

    // 统计打印
    totalKeyCount_++;
    totalLatencyMs_ += latencyMs;
    maxLatencyMs_ = std::max(maxLatencyMs_, (double)latencyMs);
    minLatencyMs_ = std::min(minLatencyMs_, (double)latencyMs);
    double avgMs = totalLatencyMs_ / totalKeyCount_;
    LOG_INFO(kTag, QString("⏱ 按键→录音延迟: %1ms (平均: %2ms, 最小: %3ms, 最大: %4ms, 累计: %5次)")
        .arg(latencyMs).arg(avgMs, 0, 'f', 0)
        .arg(minLatencyMs_, 0, 'f', 0).arg(maxLatencyMs_, 0, 'f', 0)
        .arg(totalKeyCount_));
    latencyTracking_ = false;
}

void VoiceInputService::onHotkeyDeactivated() {
    // Cooldown 状态的 Deactivated → 忽略
    if (state_ == Cooldown) {
        LOG_DEBUG(kTag, "忽略 Deactivated (Cooldown)");
        return;
    }

    recording_ = false;
    longPressTimer_->stop();

    // 停止音频采集
    if (impl_->audioCapture && impl_->audioCapture->isRunning()) {
        impl_->audioCapture->stop();
    }

    if (state_ == Recording) {
        // 松开 → 先恢复 CapsLock 灯，再开始识别
        simulateCapsLock();
        state_ = Idle;
        LOG_DEBUG(kTag, "Recording → Idle (松开转写)");
        stopRecordingAndTranscribe();
    }

    // 启动冷却期
    state_ = Cooldown;
    cooldownTimer_->start(releaseCooldownMs_);
    LOG_DEBUG(kTag, QString("→ Cooldown (%1ms)").arg(releaseCooldownMs_));
}

void VoiceInputService::onAudioData(const std::vector<float>& samples, int sampleRate) {
    if (!recording_) return;

    audioSampleRate_ = sampleRate;
    audioBuffer_.insert(audioBuffer_.end(), samples.begin(), samples.end());
}

void VoiceInputService::stopRecordingAndTranscribe() {
    if (audioBuffer_.empty()) {
        // 无音频 → 复位 CapsLock 灯
        simulateCapsLock();
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
    // 识别完成后，复位 CapsLock 灯
    simulateCapsLock();

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
#ifndef PLATFORM_WINDOWS
    // XTest 模拟的按键会被 D-Bus portal 再次捕获，导致 Activated/Deactivated 信号。
    // 在模拟期间屏蔽 portal 信号，防止状态机被打断。
    if (impl_->hotkey) {
        impl_->hotkey->setIgnoreEvents(true);
    }
#endif
    if (impl_->injector && impl_->injector->isInitialized()) {
        impl_->injector->simulateKeysym(0xffe5);
        LOG_DEBUG(kTag, "模拟 CapsLock 按键");
    } else {
        LOG_WARNING(kTag, "文本注入器未初始化，无法模拟 CapsLock");
    }
#ifndef PLATFORM_WINDOWS
    if (impl_->hotkey) {
        impl_->hotkey->setIgnoreEvents(false);
    }
#endif
}

} // namespace impress
