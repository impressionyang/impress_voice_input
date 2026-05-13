#include "stt_test_page.h"
#include "core/sense_voice_engine.h"
#include "audio/audio_capture.h"
#include "audio/streaming_audio_writer.h"
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
#include <QtConcurrent>

static const char* const kTag = "STTTestPage";

namespace impress {

STTTestPage::STTTestPage(ConfigManager* configManager,
                         SenseVoiceEngine* sttEngine,
                         QWidget* parent)
    : QWidget(parent)
    , configManager_(configManager)
    , sttEngine_(sttEngine)
    , audioCapture_(new AudioCapture(this))
    , streamingWriter_(new StreamingAudioWriter(this))
{
    setupUI();

    // 信号连接
    connect(audioCapture_, &AudioCapture::audioDataReady,
            this, &STTTestPage::onAudioDataReady);
    connect(streamingWriter_, &StreamingAudioWriter::chunkCompleted,
            this, &STTTestPage::onChunkCompleted);
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
}

void STTTestPage::onToggleRecording() {
    if (isRecording_) {
        streamingWriter_->stop();
        audioCapture_->stop();
        isRecording_ = false;
        isInferencing_ = false;
    } else {
        // 检查全局模型是否已加载
        if (!sttEngine_->isLoaded()) {
            QMessageBox::warning(this, "提示",
                "模型尚未加载完成，请稍候再试");
            return;
        }

        // 从配置同步调试开关到引擎
        sttEngine_->setDebugSaveAudio(
            configManager_->get("stt.debug_save_audio").toBool());

        startAudioCapture();
    }
    updateUIState();
}

void STTTestPage::onModelLoaded(const QString& modelPath) {
    LOG_INFO(kTag, QString("全局模型加载成功: %1").arg(modelPath));
    isLoadingModel_ = false;
    statusLabel_->setText(QString("模型就绪: %1").arg(
        QFileInfo(modelPath).fileName()));
    updateUIState();
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
    bool debugEnabled = configManager_->get("stt.debug_save_audio").toBool();

    // 启动流式录制器
    if (!streamingWriter_->start(audioSampleRate_, debugEnabled)) {
        QMessageBox::critical(this, "错误", "无法启动流式录制器");
        return;
    }

    // 启动音频采集
    if (!audioCapture_->start(deviceIdx, audioSampleRate_)) {
        streamingWriter_->stop();
        QMessageBox::critical(this, "错误", "无法启动音频采集");
        return;
    }

    isRecording_ = true;
    isInferencing_ = false;
    completedCount_ = 0;

    statusLabel_->setText("录音中 | VAD 流式识别");
    updateUIState();
}

void STTTestPage::onAudioDataReady(const std::vector<float>& samples, int /* sampleRate */) {
    // 写入流式录制器（WAV 文件 + VAD 静音检测）
    streamingWriter_->writeSamples(samples);

    // 更新波形显示
    waveform_->setSamples(samples);
}

void STTTestPage::onChunkCompleted(const QString& filePath, int durationMs) {
    completedCount_++;
    LOG_INFO(kTag, QString("WAV 片段 #%1 已完成: %2 (%3ms)")
        .arg(completedCount_).arg(filePath).arg(durationMs));

    statusLabel_->setText(QString("正在识别 #%1 (%2ms)...").arg(completedCount_).arg(durationMs));

    // 在后台线程对 WAV 文件进行识别
    transcribeChunk(filePath, durationMs);
}

void STTTestPage::transcribeChunk(const QString& filePath, int /* durationMs */) {
    if (isInferencing_) {
        // 上一个识别还没完成，跳过（避免堆积）
        LOG_WARNING(kTag, "上一个识别仍在进行中，跳过当前片段");
        return;
    }

    isInferencing_ = true;

    // 在后台线程读取 WAV 文件并推理
    (void)QtConcurrent::run([this, filePath]() {
        QString text;
        QString errorMsg;

        // 读取 WAV 文件为 float 样本
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            errorMsg = QString("无法打开 WAV 文件: %1").arg(filePath);
        } else {
            // 跳过 44 字节 WAV 头
            file.seek(44);
            QByteArray raw = file.readAll();
            file.close();

            // int16 -> float
            int numSamples = raw.size() / 2;
            std::vector<float> samples(numSamples);
            for (int i = 0; i < numSamples; i++) {
                int16_t val = *reinterpret_cast<const int16_t*>(raw.data() + i * 2);
                samples[i] = static_cast<float>(val) / 32767.0f;
            }

            if (!sttEngine_->isLoaded()) {
                text = "[错误] 模型未加载";
            } else {
                QString language = configManager_->get("stt.language").toString();
                auto result = sttEngine_->infer(samples, audioSampleRate_, language);
                text = result.text;
                errorMsg = result.text.startsWith("[错误]") ? result.text : QString();
            }
        }

        // 回到主线程更新 UI
        QMetaObject::invokeMethod(this, [this, text, errorMsg, filePath]() {
            isInferencing_ = false;

            if (!errorMsg.isEmpty() && text.startsWith("[错误]")) {
                statusLabel_->setText(text);
            } else if (text.isEmpty()) {
                statusLabel_->setText(QString("片段 #%1: 静音").arg(completedCount_));
            } else {
                emit onRecognitionResult(text, 1.0f, 0, true);
            }

            if (isRecording_) {
                statusLabel_->setText(QString("录音中 | 已识别 %1 个片段").arg(completedCount_));
            }
        }, Qt::QueuedConnection);
    });
}

void STTTestPage::onRecognitionResult(const QString& text, float confidence,
                                      double latency, bool isFinal)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line = QString("[%1] #%2 %3 (置信度: %4%, 延迟: %5 ms)")
        .arg(timestamp).arg(completedCount_).arg(text)
        .arg(confidence * 100, 0, 'f', 1)
        .arg(latency, 0, 'f', 1);

    textOutput_->append(line);
    latencyLabel_->setText(QString("延迟: %1 ms").arg(latency, 0, 'f', 1));

    if (isFinal) {
        textOutput_->append("---\n");
    }
}

} // namespace impress
