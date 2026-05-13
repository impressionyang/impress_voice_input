#include "voice_input_service.h"
#include "audio/audio_capture.h"
#include "sense_voice_engine.h"
#include "caps_lock_voice_hotkey.h"
#include "wayland_text_injector.h"
#include "app/config_manager.h"
#include "utils/logger.h"

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

VoiceInputService::VoiceInputService(ConfigManager* configManager, QObject* parent)
    : QObject(parent)
    , configManager_(configManager)
    , impl_(std::make_unique<Impl>())
{
    longPressTimer_ = new QTimer(this);
    longPressTimer_->setSingleShot(true);
    connect(longPressTimer_, &QTimer::timeout, this, [this]() {
        // 长按超时仍未松开 → 确认为长按录音
        if (!longPressDetected_) {
            longPressDetected_ = true;
            emit statusChanged("正在录音...");
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

    // 2. 初始化 STT 引擎并加载模型
    impl_->sttEngine = new SenseVoiceEngine(this);

    // 从配置加载模型
    QString modelPath = configManager_->get("stt.model_path").toString();
    QString tokensPath = configManager_->get("stt.tokens_path").toString();
    QString device = configManager_->get("stt.device").toString();
    int numThreads = configManager_->get("stt.num_threads").toInt();

    if (!modelPath.isEmpty()) {
        LOG_INFO(kTag, QString("正在加载 STT 模型: %1").arg(modelPath));
        bool modelLoaded = impl_->sttEngine->loadModelSync(modelPath, tokensPath, device, numThreads);
        if (!modelLoaded) {
            emit error(QString("STT 模型加载失败: %1").arg(modelPath));
            LOG_ERROR(kTag, "STT 模型加载失败");
        } else {
            LOG_INFO(kTag, "STT 模型加载成功");
            // 同步调试音频设置
            bool debugSave = configManager_->get("stt.debug_save_audio").toBool();
            impl_->sttEngine->setDebugSaveAudio(debugSave);
        }
    } else {
        LOG_WARNING(kTag, "模型路径为空，请先在配置中设置模型路径");
    }

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
    emit statusChanged("语音输入已启动（等待授权...）");
    LOG_INFO(kTag, "语音输入服务已启动");
    return true;
}

void VoiceInputService::stop() {
    if (!running_) return;

    longPressTimer_->stop();

    if (impl_->audioCapture) {
        impl_->audioCapture->stop();
    }
    if (impl_->sttEngine) {
        impl_->sttEngine->unloadModel();
    }
    if (impl_->hotkey) {
        impl_->hotkey->stop();
    }

    running_ = false;
    recording_ = false;
    longPressDetected_ = false;
    audioBuffer_.clear();

    LOG_INFO(kTag, "语音输入服务已停止");
}

void VoiceInputService::onHotkeyActivated() {
    LOG_DEBUG(kTag, "快捷键激活（按下）");
    recording_ = true;
    longPressDetected_ = false;
    audioBuffer_.clear();

    // 启动长按定时器
    longPressTimer_->start(longPressThreshold_);

    // 开始音频采集（后台预采集）
    int deviceIndex = -1; // 默认设备
    impl_->audioCapture->start(deviceIndex, 16000, 20);

    emit statusChanged("等待长按确认...");
}

void VoiceInputService::onHotkeyDeactivated() {
    LOG_DEBUG(kTag, "快捷键停用（松开）");
    recording_ = false;
    longPressTimer_->stop();

    // 停止音频采集
    if (impl_->audioCapture && impl_->audioCapture->isRunning()) {
        impl_->audioCapture->stop();
    }

    if (!longPressDetected_) {
        // 短按 → 模拟 CapsLock 按键
        LOG_DEBUG(kTag, "短按，模拟 CapsLock");
        simulateCapsLock();
        emit statusChanged("短按：切换 CapsLock");
    } else {
        // 长按 → 停止录音并转写
        stopRecordingAndTranscribe();
    }

    longPressDetected_ = false;
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
        // CapsLock keysym = 0xffe5
        unsigned int capslockKeysym = 0xffe5;
        impl_->injector->simulateKeycode(capslockKeysym);
        LOG_DEBUG(kTag, "模拟 CapsLock 按键已注入");
    } else {
        LOG_WARNING(kTag, "文本注入器未初始化，无法模拟 CapsLock");
    }
}

} // namespace impress
