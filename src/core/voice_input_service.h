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
 *   按下 → 灯亮 (PreRecording) → 1s 后开始正式录音 (Recording，灯保持亮)
 *   → 松开 → 识别 → 注入文本 → 复位 CapsLock 灯
 *
 * 状态完全通过托盘图标指示：
 *   绿色 ○ — 就绪（静默）
 *   黄色 ○ — 等待长按确认
 *   红色 ● — 正在录音
 *   橙色 ◉ — 正在识别
 *
 * 状态机：
 *   Idle          — 空闲
 *   PreRecording  — 按下，灯亮，等待长按确认
 *   Recording     — 1s 后正式录音（屏蔽 Portal 信号，灯保持亮）
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
