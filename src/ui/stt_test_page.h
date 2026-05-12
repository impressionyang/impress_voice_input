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

/**
 * @brief STT 测试页面
 *
 * 实时麦克风采集 + 流式识别。
 * 使用 SenseVoice 模型进行推理。
 * 模型异步加载，不阻塞 UI。
 */
class STTTestPage : public QWidget {
    Q_OBJECT
public:
    explicit STTTestPage(ConfigManager* configManager, QWidget* parent = nullptr);
    ~STTTestPage() override;

private slots:
    void onToggleRecording();
    void onAudioDataReady(const std::vector<float>& samples, int sampleRate);
    void onRecognitionResult(const QString& text, float confidence, double latency, bool isFinal);
    void onModelLoaded(const QString& modelPath);
    void onModelLoadError(const QString& modelPath, const QString& error);
    void onModelUnloaded();

private:
    void setupUI();
    void updateUIState();
    void startAudioCapture();
    void processAudioChunk(const std::vector<float>& samples, int sampleRate);

    ConfigManager* configManager_;
    SenseVoiceEngine* sttEngine_;
    AudioCapture* audioCapture_;

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
    std::vector<float> chunkBuffer_;
    QString currentModelPath_;
};

} // namespace impress
