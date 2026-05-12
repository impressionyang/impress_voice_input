#pragma once

#include <QWidget>
#include <memory>

class QLabel;
class QPushButton;
class QTextEdit;
class QProgressBar;
class QListWidget;
class QComboBox;

namespace impress {

class ConfigManager;
class STTEngine;
class AudioDecoder;

struct TranscribeTask {
    QString filePath;
    QString status; // "等待中", "处理中", "完成", "失败"
    QString result;
    double progress = 0.0;
};

/**
 * @brief 音频文件转写页面
 *
 * 支持单文件/批量转写，进度显示，结果导出。
 */
class FileTranscribePage : public QWidget {
    Q_OBJECT
public:
    explicit FileTranscribePage(ConfigManager* configManager, QWidget* parent = nullptr);
    ~FileTranscribePage() override;

private slots:
    void onAddFiles();
    void onClearQueue();
    void onStartTranscribe();
    void onStopTranscribe();
    void onExportResult();

private:
    void setupUI();
    void updateUIState();
    void processNextFile();

    ConfigManager* configManager_;
    STTEngine* sttEngine_;
    AudioDecoder* audioDecoder_;

    // UI 控件
    QListWidget* fileList_;
    QPushButton* addBtn_;
    QPushButton* clearBtn_;
    QPushButton* startBtn_;
    QPushButton* stopBtn_;
    QPushButton* exportBtn_;
    QTextEdit* resultText_;
    QProgressBar* progressBar_;
    QLabel* statusLabel_;
    QComboBox* exportFormat_;

    bool isTranscribing_ = false;
    QList<TranscribeTask> tasks_;
    int currentTaskIndex_ = -1;
};

} // namespace impress
