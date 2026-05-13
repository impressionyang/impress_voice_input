#pragma once

#include <QWidget>
#include <memory>
#include "widgets/audio_waveform.h"

class QLabel;
class QPushButton;
class QComboBox;
class QTextEdit;
class QSpinBox;

namespace impress {

class ConfigManager;
class SenseVoiceEngine;
class AudioCapture;
class StreamingAudioWriter;

/**
 * @brief STT 测试页面
 *
 * 实时麦克风采集 + 基于 VAD 的流式 WAV 文件录制 + 后台识别。
 * 音频采集与推理分离，防止推理阻塞音频流。
 * 使用 VAD 检测静音段自动切换 WAV 文件，每个文件完成后触发识别。
 */
class STTTestPage : public QWidget {
    Q_OBJECT
public:
    explicit STTTestPage(ConfigManager* configManager,
                         SenseVoiceEngine* sttEngine,
                         QWidget* parent = nullptr);
    ~STTTestPage() override;

private slots:
    void onToggleRecording();
    void onAudioDataReady(const std::vector<float>& samples, int sampleRate);
    void onChunkCompleted(const QString& filePath, int durationMs);
    void onRecognitionResult(const QString& text, float confidence, double latency, bool isFinal);
    void onModelLoaded(const QString& modelPath);
    void onModelLoadError(const QString& modelPath, const QString& error);
    void onModelUnloaded();

private:
    void setupUI();
    void updateUIState();
    void startAudioCapture();
    void transcribeChunk(const QString& filePath, int durationMs);

    ConfigManager* configManager_;
    SenseVoiceEngine* sttEngine_;
    AudioCapture* audioCapture_;
    StreamingAudioWriter* streamingWriter_;

    // UI 控件
    QComboBox* deviceCombo_;
    QPushButton* recordBtn_;
    QTextEdit* textOutput_;
    QLabel* latencyLabel_;
    QLabel* statusLabel_;
    AudioWaveform* waveform_;

    bool isRecording_ = false;
    bool isLoadingModel_ = false;
    bool isInferencing_ = false;
    int audioSampleRate_ = 16000;
    int completedCount_ = 0;  // 已完成文件计数
};

} // namespace impress
