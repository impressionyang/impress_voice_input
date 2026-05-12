#include "stt_test_page.h"
#include "core/sense_voice_engine.h"
#include "audio/audio_capture.h"
#include "audio/audio_ring_buffer.h"
#include "widgets/audio_waveform.h"
#include "app/config_manager.h"
#include "utils/logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QSpinBox>
#include <QMessageBox>
#include <QDateTime>
#include <QFileInfo>

static const char* const kTag = "STTTestPage";

namespace impress {

STTTestPage::STTTestPage(ConfigManager* configManager, QWidget* parent)
    : QWidget(parent)
    , configManager_(configManager)
    , sttEngine_(new SenseVoiceEngine(this))
    , audioCapture_(new AudioCapture(this))
{
    setupUI();

    // 信号连接
    connect(audioCapture_, &AudioCapture::audioDataReady,
            this, &STTTestPage::onAudioDataReady);
    connect(sttEngine_, &SenseVoiceEngine::modelLoaded,
            this, &STTTestPage::onModelLoaded);
    connect(sttEngine_, &SenseVoiceEngine::modelLoadError,
            this, &STTTestPage::onModelLoadError);
    connect(sttEngine_, &SenseVoiceEngine::modelUnloaded,
            this, &STTTestPage::onModelUnloaded);
}

STTTestPage::~STTTestPage() = default;

void STTTestPage::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // 控制面板
    auto* controlGroup = new QGroupBox("控制面板", this);
    auto* controlLayout = new QFormLayout(controlGroup);

    deviceCombo_ = new QComboBox(this);
    deviceCombo_->addItems(AudioCapture::getDeviceList());
    controlLayout->addRow("输入设备:", deviceCombo_);

    chunkSizeSpin_ = new QSpinBox(this);
    chunkSizeSpin_->setRange(500, 10000);
    chunkSizeSpin_->setSingleStep(500);
    chunkSizeSpin_->setValue(3000);
    chunkSizeSpin_->setSuffix(" ms");
    controlLayout->addRow("推理间隔:", chunkSizeSpin_);

    auto* btnLayout = new QHBoxLayout();
    recordBtn_ = new QPushButton("开始录音", this);
    recordBtn_->setMinimumWidth(120);
    recordBtn_->setStyleSheet("QPushButton { font-weight: bold; padding: 8px 16px; }");
    connect(recordBtn_, &QPushButton::clicked, this, &STTTestPage::onToggleRecording);
    btnLayout->addWidget(recordBtn_);

    statusLabel_ = new QLabel("就绪", this);
    statusLabel_->setStyleSheet("color: gray;");
    btnLayout->addWidget(statusLabel_);
    btnLayout->addStretch();

    controlLayout->addRow(btnLayout);
    mainLayout->addWidget(controlGroup);

    // 状态信息
    auto* infoLayout = new QHBoxLayout();
    latencyLabel_ = new QLabel("延迟: -- ms", this);
    latencyLabel_->setStyleSheet("font-family: monospace; font-size: 13px;");
    infoLayout->addWidget(latencyLabel_);
    infoLayout->addStretch();
    mainLayout->addLayout(infoLayout);

    // 波形
    waveform_ = new AudioWaveform(this);
    waveform_->setMinimumHeight(80);
    mainLayout->addWidget(waveform_);

    // 文本输出
    auto* outputGroup = new QGroupBox("识别结果", this);
    auto* outputLayout = new QVBoxLayout(outputGroup);
    textOutput_ = new QTextEdit(this);
    textOutput_->setReadOnly(true);
    textOutput_->setFont(QFont("Monospace", 12));
    outputLayout->addWidget(textOutput_);
    mainLayout->addWidget(outputGroup);

    updateUIState();
}

void STTTestPage::updateUIState() {
    recordBtn_->setText(isRecording_ ? "停止录音" : "开始录音");
    recordBtn_->setStyleSheet(isRecording_
        ? "QPushButton { font-weight: bold; padding: 8px 16px; background-color: #e74c3c; color: white; }"
        : "QPushButton { font-weight: bold; padding: 8px 16px; }");
    deviceCombo_->setEnabled(!isRecording_ && !isLoadingModel_);
    chunkSizeSpin_->setEnabled(!isRecording_ && !isLoadingModel_);
}

void STTTestPage::onToggleRecording() {
    if (isRecording_) {
        audioCapture_->stop();
        sttEngine_->unloadModel();
        isRecording_ = false;
    } else {
        // 读取配置
        QString modelPath = configManager_->get("stt.model_path").toString();
        if (modelPath.isEmpty()) {
            QMessageBox::warning(this, "提示",
                "请先在「配置」页面设置模型路径并保存");
            return;
        }

        // 异步加载模型
        if (!sttEngine_->isLoaded() ||
            currentModelPath_ != modelPath) {
            isLoadingModel_ = true;
            statusLabel_->setText("正在加载模型，请稍候...");
            updateUIState();

            sttEngine_->loadModelAsync(modelPath,
                configManager_->get("stt.tokens_path").toString(),
                configManager_->get("stt.device").toString(),
                configManager_->get("stt.num_threads").toInt());

            currentModelPath_ = modelPath;
            // 注意：startAudioCapture() 将在 onModelLoaded() 回调中调用
        } else {
            startAudioCapture();
        }
    }
    updateUIState();
}

void STTTestPage::onModelLoaded(const QString& modelPath) {
    LOG_INFO(kTag, QString("模型加载成功: %1").arg(modelPath));
    isLoadingModel_ = false;
    statusLabel_->setText(QString("模型就绪: %1").arg(
        QFileInfo(modelPath).fileName()));
    updateUIState();

    // 如果用户仍在录音状态（已切换 UI），启动采集
    if (!isRecording_) {
        startAudioCapture();
    }
}

void STTTestPage::onModelLoadError(const QString& modelPath, const QString& error) {
    LOG_ERROR(kTag, QString("模型加载失败: %1 - %2").arg(modelPath, error));
    isLoadingModel_ = false;
    statusLabel_->setText("模型加载失败");
    updateUIState();

    QMessageBox::critical(this, "模型加载错误",
        QString("无法加载模型文件:\n%1\n\n错误信息:\n%2")
        .arg(modelPath, error));
}

void STTTestPage::onModelUnloaded() {
    isLoadingModel_ = false;
    statusLabel_->setText("模型已卸载");
}

void STTTestPage::startAudioCapture() {
    int deviceIdx = deviceCombo_->currentIndex() - 1;
    int sampleRate = configManager_->get("stt.sample_rate").toInt();

    if (!audioCapture_->start(deviceIdx, sampleRate)) {
        QMessageBox::critical(this, "错误", "无法启动音频采集");
        return;
    }
    isRecording_ = true;
    statusLabel_->setText(QString("录音中 | 模型: %1").arg(
        QFileInfo(currentModelPath_).fileName()));
    updateUIState();
}

void STTTestPage::onAudioDataReady(const std::vector<float>& samples, int sampleRate) {
    chunkBuffer_.insert(chunkBuffer_.end(), samples.begin(), samples.end());

    int chunkSize = configManager_->get("stt.sample_rate").toInt()
                  * chunkSizeSpin_->value() / 1000;

    if (static_cast<int>(chunkBuffer_.size()) >= chunkSize) {
        std::vector<float> chunk(chunkBuffer_.begin(), chunkBuffer_.begin() + chunkSize);
        chunkBuffer_.erase(chunkBuffer_.begin(), chunkBuffer_.begin() + chunkSize);

        waveform_->setSamples(samples);
        processAudioChunk(chunk, sampleRate);
    } else {
        waveform_->setSamples(samples);
    }
}

void STTTestPage::processAudioChunk(const std::vector<float>& samples, int sampleRate) {
    // 模型已在 onToggleRecording 中异步加载，此处防御性检查
    if (!sttEngine_->isLoaded()) {
        return;
    }

    auto result = sttEngine_->infer(samples, sampleRate,
        configManager_->get("stt.language").toString());
    emit onRecognitionResult(result.text, result.confidence, result.latency_ms, result.isFinal);
}

void STTTestPage::onRecognitionResult(const QString& text, float confidence,
                                      double latency, bool isFinal)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line = QString("[%1] %2 (置信度: %3%, 延迟: %4 ms)\n")
        .arg(timestamp, text)
        .arg(confidence * 100, 0, 'f', 1)
        .arg(latency, 0, 'f', 1);

    textOutput_->append(line);
    latencyLabel_->setText(QString("延迟: %1 ms").arg(latency, 0, 'f', 1));

    if (isFinal) {
        textOutput_->append("---\n");
    }
}

} // namespace impress
