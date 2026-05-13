#pragma once

#include <QWidget>
#include <memory>
#include "widgets/audio_waveform.h"

class QLabel;
class QPushButton;
class QComboBox;
class QTextEdit;
class QSpinBox;
class QTimer;

namespace impress {

class ConfigManager;
class SenseVoiceEngine;
class AudioCapture;

/**
 * @brief STT 测试页面
 *
 * 实时麦克风采集 + 周期性后台推理。
 * 音频采集与推理分离，防止推理阻塞音频流。
 * 使用 SenseVoice 模型进行推理。
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
    void onRecognitionResult(const QString& text, float confidence, double latency, bool isFinal);
    void onModelLoaded(const QString& modelPath);
    void onModelLoadError(const QString& modelPath, const QString& error);
    void onModelUnloaded();
    void onInferenceTimer();

private:
    void setupUI();
    void updateUIState();
    void startAudioCapture();
    void startInferenceTimer();

    ConfigManager* configManager_;
    SenseVoiceEngine* sttEngine_;
    AudioCapture* audioCapture_;
    QTimer* inferenceTimer_;

    // UI 控件
    QComboBox* deviceCombo_;
    QPushButton* recordBtn_;
    QTextEdit* textOutput_;
    QLabel* latencyLabel_;
    QLabel* statusLabel_;
    AudioWaveform* waveform_;
    QSpinBox* chunkSizeSpin_;

    bool isRecording_ = false;
    bool isLoadingModel_ = false;
    bool isInferencing_ = false;
    int audioSampleRate_ = 16000;
    std::vector<float> audioBuffer_;
};

} // namespace impress
