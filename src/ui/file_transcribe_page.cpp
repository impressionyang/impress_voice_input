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
#include <QFuture>
#include <QtConcurrent>

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

    // 在后台线程中加载模型（不阻塞 UI）
    statusLabel_->setText("正在加载模型...");
    startBtn_->setEnabled(false);
    activeWorkers_ = 1; // 标记正在加载模型

    (void)QtConcurrent::run([this, modelPath]() {
        bool success = sttEngine_->loadModelSync(modelPath,
            configManager_->get("stt.device").toString(),
            configManager_->get("stt.num_threads").toInt());

        QMetaObject::invokeMethod(this, [this, success]() {
            activeWorkers_--;
            if (!success) {
                QMessageBox::critical(this, "错误", "模型加载失败");
                statusLabel_->setText("模型加载失败");
                startBtn_->setEnabled(true);
                return;
            }

            isTranscribing_ = true;
            currentTaskIndex_ = 0;
            progressBar_->setVisible(true);
            updateUIState();
            statusLabel_->setText("开始批量转写...");

            // 启动后台转写队列
            startBatchTranscription();
        }, Qt::QueuedConnection);
    });
}

void FileTranscribePage::onStopTranscribe() {
    isTranscribing_ = false;
    activeWorkers_ = 0;
    progressBar_->setVisible(false);
    statusLabel_->setText("已停止");
    sttEngine_->unloadModel();
    updateUIState();
}

void FileTranscribePage::startBatchTranscription() {
    // 使用单线程队列处理，避免内存占用过高
    processFileAsync(currentTaskIndex_);
}

void FileTranscribePage::processFileAsync(int index) {
    if (!isTranscribing_ || index >= tasks_.size()) {
        return;
    }

    auto& task = tasks_[index];
    task.status = "处理中";
    statusLabel_->setText(QString("正在处理: %1 (%2/%3)")
        .arg(QFileInfo(task.filePath).fileName())
        .arg(index + 1)
        .arg(tasks_.size()));

    activeWorkers_++;

    // 在后台线程中执行解码和推理
    (void)QtConcurrent::run([this, index, taskFile = task.filePath]() {
        QString text;
        bool success = false;

        // 创建独立的解码器和引擎实例（避免线程冲突）
        AudioDecoder decoder;
        if (decoder.decode(taskFile)) {
            const auto& samples = decoder.samples();
            int sampleRate = decoder.sampleRate();

            // 使用已加载的引擎进行推理（引擎是线程安全的）
            auto result = sttEngine_->infer(samples, sampleRate,
                configManager_->get("stt.language").toString());
            text = result.text;
            success = true;
        }

        // 回到主线程更新 UI
        QMetaObject::invokeMethod(this, [this, index, text, success]() {
            activeWorkers_--;
            onTaskComplete(index, text, success);
        }, Qt::QueuedConnection);
    });
}

void FileTranscribePage::onTaskComplete(int index, const QString& text, bool success) {
    if (index >= tasks_.size()) return;

    auto& task = tasks_[index];
    task.result = text;
    task.status = success ? "完成" : "失败";
    task.progress = 1.0;

    if (success) {
        resultText_->append(
            QString("=== %1 ===\n%2\n").arg(
                QFileInfo(task.filePath).fileName(), text));
    } else {
        resultText_->append(
            QString("=== %1 === [失败]\n").arg(QFileInfo(task.filePath).fileName()));
    }

    // 更新列表项
    auto* item = fileList_->item(index);
    if (item) {
        item->setText(QString("%1 — %2")
            .arg(QFileInfo(task.filePath).fileName(), task.status));
    }

    currentTaskIndex_ = index + 1;
    progressBar_->setValue(
        static_cast<int>(currentTaskIndex_ * 100.0 / tasks_.size()));

    // 继续下一个
    if (isTranscribing_ && currentTaskIndex_ < tasks_.size()) {
        processFileAsync(currentTaskIndex_);
    } else {
        onAllComplete();
    }
}

void FileTranscribePage::onAllComplete() {
    isTranscribing_ = false;
    statusLabel_->setText("全部完成");
    progressBar_->setVisible(false);
    sttEngine_->unloadModel();
    updateUIState();
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
