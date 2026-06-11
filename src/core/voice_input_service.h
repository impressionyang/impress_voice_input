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
 * 状态机（防止 CapsLock 抖动/误触）：
 *  Idle          — 空闲，等待按键
 *  PreRecording  — 按下 CapsLock，预录音，等待长按确认
 *  Recording     — 长按 1s 确认，正式录音（屏蔽所有 Portal 信号）
 *  Cooldown      — 松开后冷却期，防止立即重新触发
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
    enum State { Idle, PreRecording, Recording, Cooldown };
    State state_ = Idle;

    struct Impl;
    ConfigManager* configManager_ = nullptr;
    std::unique_ptr<Impl> impl_;

    bool running_ = false;
    bool recording_ = false;
    int longPressThreshold_ = 1000;
    int releaseCooldownMs_ = 1000;

    std::vector<float> audioBuffer_;
    int audioSampleRate_ = 16000;

    QTimer* longPressTimer_ = nullptr;
    QTimer* cooldownTimer_ = nullptr;

    void startRecording();
    void stopRecordingAndTranscribe();
    void simulateCapsLock();
};

} // namespace impress
