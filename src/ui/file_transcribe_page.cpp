#include "file_transcribe_page.h"
#include "core/stt_engine.h"
#include "audio/audio_decoder.h"
#include "app/config_manager.h"
#include "utils/logger.h"
#include "utils/string_utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTextEdit>
#include <QProgressBar>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QFileInfo>

static const char* const kTag = "FileTranscribePage";

namespace impress {

FileTranscribePage::FileTranscribePage(ConfigManager* configManager, QWidget* parent)
    : QWidget(parent)
    , configManager_(configManager)
    , sttEngine_(new STTEngine(this))
    , audioDecoder_(new AudioDecoder(this))
{
    setupUI();
}

FileTranscribePage::~FileTranscribePage() = default;

void FileTranscribePage::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // 文件队列
    auto* queueGroup = new QGroupBox("文件队列", this);
    auto* queueLayout = new QVBoxLayout(queueGroup);

    fileList_ = new QListWidget(this);
    fileList_->setMinimumHeight(120);
    queueLayout->addWidget(fileList_);

    auto* btnLayout = new QHBoxLayout();
    addBtn_ = new QPushButton("添加文件", this);
    connect(addBtn_, &QPushButton::clicked, this, &FileTranscribePage::onAddFiles);
    btnLayout->addWidget(addBtn_);

    clearBtn_ = new QPushButton("清空队列", this);
    connect(clearBtn_, &QPushButton::clicked, this, &FileTranscribePage::onClearQueue);
    btnLayout->addWidget(clearBtn_);
    btnLayout->addStretch();
    queueLayout->addLayout(btnLayout);

    mainLayout->addWidget(queueGroup);

    // 控制栏
    auto* controlLayout = new QHBoxLayout();
    startBtn_ = new QPushButton("开始转写", this);
    startBtn_->setStyleSheet("QPushButton { font-weight: bold; padding: 8px 16px; }");
    connect(startBtn_, &QPushButton::clicked, this, &FileTranscribePage::onStartTranscribe);
    controlLayout->addWidget(startBtn_);

    stopBtn_ = new QPushButton("停止", this);
    stopBtn_->setEnabled(false);
    connect(stopBtn_, &QPushButton::clicked, this, &FileTranscribePage::onStopTranscribe);
    controlLayout->addWidget(stopBtn_);

    controlLayout->addWidget(new QLabel("导出格式:", this));
    exportFormat_ = new QComboBox(this);
    exportFormat_->addItems({"TXT", "SRT (字幕)", "JSON"});
    controlLayout->addWidget(exportFormat_);

    exportBtn_ = new QPushButton("导出结果", this);
    connect(exportBtn_, &QPushButton::clicked, this, &FileTranscribePage::onExportResult);
    controlLayout->addWidget(exportBtn_);

    mainLayout->addLayout(controlLayout);

    // 进度
    progressBar_ = new QProgressBar(this);
    progressBar_->setVisible(false);
    mainLayout->addWidget(progressBar_);

    statusLabel_ = new QLabel("就绪", this);
    statusLabel_->setStyleSheet("color: gray;");
    mainLayout->addWidget(statusLabel_);

    // 结果
    auto* resultGroup = new QGroupBox("转写结果", this);
    auto* resultLayout = new QVBoxLayout(resultGroup);
    resultText_ = new QTextEdit(this);
    resultText_->setReadOnly(true);
    resultText_->setFont(QFont("Monospace", 11));
    resultLayout->addWidget(resultText_);
    mainLayout->addWidget(resultGroup);
}

void FileTranscribePage::updateUIState() {
    startBtn_->setEnabled(!isTranscribing_ && !tasks_.isEmpty());
    stopBtn_->setEnabled(isTranscribing_);
    addBtn_->setEnabled(!isTranscribing_);
    clearBtn_->setEnabled(!isTranscribing_);
    exportBtn_->setEnabled(!isTranscribing_);
}

void FileTranscribePage::onAddFiles() {
    QStringList formats;
    for (const auto& fmt : AudioDecoder::supportedFormats()) {
        formats << QString("*.%1").arg(fmt);
    }
    QString filter = QString("音频文件 (%1)").arg(formats.join(" "));

    QStringList files = QFileDialog::getOpenFileNames(this, "选择音频文件", "", filter);
    for (const auto& file : files) {
        TranscribeTask task;
        task.filePath = file;
        task.status = "等待中";
        tasks_.append(task);

        auto* item = new QListWidgetItem(
            QString("%1 — 等待中").arg(QFileInfo(file).fileName()));
        fileList_->addItem(item);
    }
}

void FileTranscribePage::onClearQueue() {
    tasks_.clear();
    fileList_->clear();
    statusLabel_->setText("队列已清空");
}

void FileTranscribePage::onStartTranscribe() {
    if (tasks_.isEmpty()) {
        QMessageBox::information(this, "提示", "请先添加音频文件");
        return;
    }

    QString modelPath = configManager_->get("stt.model_path").toString();
    if (modelPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先在配置页面设置模型路径");
        return;
    }

    if (!sttEngine_->loadModelSync(modelPath,
        configManager_->get("stt.device").toString(),
        configManager_->get("stt.num_threads").toInt()))
    {
        QMessageBox::critical(this, "错误", "模型加载失败");
        return;
    }

    isTranscribing_ = true;
    currentTaskIndex_ = 0;
    progressBar_->setVisible(true);
    updateUIState();
    processNextFile();
}

void FileTranscribePage::onStopTranscribe() {
    isTranscribing_ = false;
    progressBar_->setVisible(false);
    statusLabel_->setText("已停止");
    updateUIState();
}

void FileTranscribePage::processNextFile() {
    if (!isTranscribing_ || currentTaskIndex_ >= tasks_.size()) {
        isTranscribing_ = false;
        statusLabel_->setText("全部完成");
        progressBar_->setVisible(false);
        updateUIState();
        return;
    }

    auto& task = tasks_[currentTaskIndex_];
    task.status = "处理中";
    statusLabel_->setText(QString("正在处理: %1").arg(QFileInfo(task.filePath).fileName()));

    // TODO: 在后台线程中执行解码和推理
    // 当前为占位实现
    if (audioDecoder_->decode(task.filePath)) {
        const auto& samples = audioDecoder_->samples();
        int sampleRate = audioDecoder_->sampleRate();

        auto result = sttEngine_->infer(samples, sampleRate, false);
        task.result = result.text;
        task.status = "完成";
        task.progress = 1.0;

        resultText_->append(
            QString("=== %1 ===\n%2\n").arg(
                QFileInfo(task.filePath).fileName(), result.text));
    } else {
        task.status = "失败";
    }

    // 更新列表项
    auto* item = fileList_->item(currentTaskIndex_);
    if (item) {
        item->setText(QString("%1 — %2")
            .arg(QFileInfo(task.filePath).fileName(), task.status));
    }

    currentTaskIndex_++;
    progressBar_->setValue(
        static_cast<int>(currentTaskIndex_ * 100.0 / tasks_.size()));

    // 继续下一个
    if (isTranscribing_) {
        processNextFile();
    }
}

void FileTranscribePage::onExportResult() {
    if (resultText_->toPlainText().isEmpty()) {
        QMessageBox::information(this, "提示", "没有可导出的结果");
        return;
    }

    QString format = exportFormat_->currentText();
    QString ext = (format == "TXT") ? "txt" : (format == "JSON") ? "json" : "srt";
    QString filter = QString("%1 文件 (*.%2)").arg(format, ext);

    QString path = QFileDialog::getSaveFileName(this, "导出结果", "", filter);
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(resultText_->toPlainText().toUtf8());
        file.close();
        statusLabel_->setText(QString("已导出: %1").arg(path));
    }
}

} // namespace impress
