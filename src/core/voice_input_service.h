#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
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
 * 流程：
 *   按下 CapsLock → 立即开始录音（Recording）
 *   → 松开 → 识别 → 注入文本 → 复位 CapsLock 灯
 *
 * 托盘图标指示：
 *   绿色 ○ — 就绪（静默）
 *   红色 ● — 正在录音
 *   橙色 ◉ — 正在识别
 *
 * 状态机：
 *   Idle          — 空闲
 *   Recording     — 按键按下，正式录音（屏蔽后续 Activated 信号）
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

    // 按键到录音延迟统计
    QElapsedTimer hotkeyLatencyTimer_;
    bool latencyTracking_ = false;

    int totalKeyCount_ = 0;
    double totalLatencyMs_ = 0;
    double maxLatencyMs_ = 0;
    double minLatencyMs_ = 9999;

    void startRecording();
    void stopRecordingAndTranscribe();
    void simulateCapsLock();
};

} // namespace impress
