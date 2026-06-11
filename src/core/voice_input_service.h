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
 * CapsLock 灯作为录音状态指示器：
 *   按下 → 灯亮 (PreRecording) → 1s 后灯灭 → 正式录音 (Recording)
 *   → 松开 → 识别 → 注入 → 复位 CapsLock 状态
 *
 * 状态机：
 *   Idle          — 空闲
 *   PreRecording  — 按下，灯亮，等待长按确认
 *   Recording     — 1s 后灯灭，正式录音（屏蔽 Portal 信号）
 *   Cooldown      — 松开后冷却，防止误触
 */
class VoiceInputService : public QObject {
    Q_OBJECT
public:
    explicit VoiceInputService(ConfigManager* configManager,
                               SenseVoiceEngine* sttEngine,
                               QObject* parent = nullptr);
    ~VoiceInputService() override;

    bool start();
    void stop();

    bool isRunning() const { return running_; }
    bool isRecording() const { return recording_; }

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
