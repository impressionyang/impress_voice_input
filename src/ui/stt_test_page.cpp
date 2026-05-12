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
#include <QTimer>
#include <QtConcurrent>

static const char* const kTag = "STTTestPage";

namespace impress {

STTTestPage::STTTestPage(ConfigManager* configManager, QWidget* parent)
    : QWidget(parent)
    , configManager_(configManager)
    , sttEngine_(new SenseVoiceEngine(this))
    , audioCapture_(new AudioCapture(this))
    , inferenceTimer_(new QTimer(this))
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

    // 推理定时器：周期性触发后台推理
    connect(inferenceTimer_, &QTimer::timeout,
            this, &STTTestPage::onInferenceTimer);
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
        inferenceTimer_->stop();
        sttEngine_->unloadModel();
        isRecording_ = false;
        isInferencing_ = false;
        audioBuffer_.clear();
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
    isInferencing_ = false;
    statusLabel_->setText("模型已卸载");
}

void STTTestPage::startAudioCapture() {
    int deviceIdx = deviceCombo_->currentIndex() - 1;
    audioSampleRate_ = configManager_->get("stt.sample_rate").toInt();

    if (!audioCapture_->start(deviceIdx, audioSampleRate_)) {
        QMessageBox::critical(this, "错误", "无法启动音频采集");
        return;
    }

    isRecording_ = true;
    audioBuffer_.clear();
    isInferencing_ = false;

    // 启动周期性推理定时器
    startInferenceTimer();

    statusLabel_->setText(QString("录音中 | 模型: %1").arg(
        QFileInfo(currentModelPath_).fileName()));
    updateUIState();
}

void STTTestPage::startInferenceTimer() {
    int interval = chunkSizeSpin_->value(); // 与推理间隔同步
    inferenceTimer_->start(interval);
}

void STTTestPage::onAudioDataReady(const std::vector<float>& samples, int /* sampleRate */) {
    // 仅缓存音频数据，不直接调用推理
    // 避免推理阻塞音频采集线程
    audioBuffer_.insert(audioBuffer_.end(), samples.begin(), samples.end());

    // 更新波形显示（使用最新数据片段）
    waveform_->setSamples(samples);
}

void STTTestPage::onInferenceTimer() {
    if (!sttEngine_->isLoaded() || isInferencing_) {
        return;
    }

    int chunkSize = audioSampleRate_ * chunkSizeSpin_->value() / 1000;

    if (static_cast<int>(audioBuffer_.size()) < chunkSize) {
        return; // 缓冲区数据不足，等待下一次
    }

    // 提取一个推理块的音频
    std::vector<float> chunk(audioBuffer_.begin(), audioBuffer_.begin() + chunkSize);
    audioBuffer_.erase(audioBuffer_.begin(), audioBuffer_.begin() + chunkSize);

    // 在后台线程执行推理
    isInferencing_ = true;
    statusLabel_->setText("推理中...");

    int sampleRate = audioSampleRate_;
    QString language = configManager_->get("stt.language").toString();

    (void)QtConcurrent::run([this, chunk, sampleRate, language]() {
        auto result = sttEngine_->infer(chunk, sampleRate, language);

        // 回到主线程更新 UI
        QMetaObject::invokeMethod(this, [this, result]() {
            isInferencing_ = false;

            if (result.text.isEmpty() && !result.text.isNull()) {
                // 静音段
                latencyLabel_->setText(QString("延迟: %1 ms").arg(result.latency_ms, 0, 'f', 1));
            } else {
                emit onRecognitionResult(result.text, result.confidence,
                                          result.latency_ms, result.isFinal);
            }

            // 更新状态
            if (isRecording_) {
                int bufMs = (audioSampleRate_ > 0)
                    ? static_cast<int>(audioBuffer_.size() * 1000 / audioSampleRate_)
                    : 0;
                statusLabel_->setText(
                    QString("录音中 | 缓冲区: %1 ms").arg(bufMs));
            }
        }, Qt::QueuedConnection);
    });
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
