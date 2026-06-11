#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <vector>
#include <memory>

namespace impress {

class AudioCapture;
class SenseVoiceEngine;
class CapsLockVoiceHotkey;
class WaylandTextInjector;
class ConfigManager;

/**
 * @brief CapsLock 语音输入服务
 *
 * 协调全局快捷键、音频采集、STT 推理和文本注入。
 * 状态机：
 * 1. 按下 CapsLock → 开始预录音
 * 2. 长按超过阈值（默认 1s）→ 立即复位 CapsLock，正式录音
 * 3. 松开 CapsLock → 停止录音 → 推理 → 注入文本
 * 4. 短按（< 阈值）→ 注入 CapsLock 按键（切换大小写）
 */
class VoiceInputService : public QObject {
    Q_OBJECT
public:
    explicit VoiceInputService(ConfigManager* configManager,
                               SenseVoiceEngine* sttEngine,
                               QObject* parent = nullptr);
    ~VoiceInputService() override;

    /** @brief 启动服务（初始化所有组件） */
    bool start();

    /** @brief 停止服务 */
    void stop();

    /** @brief 是否已启动 */
    bool isRunning() const { return running_; }

    /** @brief 是否正在录音 */
    bool isRecording() const { return recording_; }

    /** @brief 长按阈值（毫秒），默认 1000ms */
    void setLongPressThreshold(int ms) { longPressThreshold_ = ms; }
    int longPressThreshold() const { return longPressThreshold_; }

signals:
    void statusChanged(const QString& status);
    void recognitionResult(const QString& text);
    void error(const QString& message);

private slots:
    void onHotkeyActivated();
    void onHotkeyDeactivated();
    void onAudioData(const std::vector<float>& samples, int sampleRate);
    void onRecognitionComplete(const QString& text);

private:
    struct Impl;
    ConfigManager* configManager_ = nullptr;
    std::unique_ptr<Impl> impl_;

    bool running_ = false;
    bool recording_ = false;
    bool longPressDetected_ = false;
    bool capsResetDone_ = false;  // CapsLock 复位后忽略重复 Activated
    bool cooldownActive_ = false;  // 松开后的冷却期，防止立即重新触发
    int longPressThreshold_ = 1000;
    int releaseCooldownMs_ = 1000;  // 松开后冷却时间

    std::vector<float> audioBuffer_;
    int audioSampleRate_ = 16000;

    QTimer* longPressTimer_ = nullptr;
    QTimer* cooldownTimer_ = nullptr;

    void startRecording();
    void stopRecordingAndTranscribe();
    void simulateCapsLock();
};

} // namespace impress
