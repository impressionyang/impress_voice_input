#pragma once

#include <QWidget>
#include <memory>

class QLabel;
class QPushButton;
class QTextEdit;
class QProgressBar;
class QListWidget;
class QComboBox;
class QByteArray;

namespace impress {

class ConfigManager;
class SenseVoiceEngine;
class AudioDecoder;

struct TranscribeTask {
    QString filePath;
    QString status; // "等待中", "处理中", "完成", "失败"
    QString result;
    double progress = 0.0;
    double durationSec = 0.0;     // 音频时长（秒）
    int sampleRate = 0;            // 采样率
    int channels = 0;              // 声道数
};

/**
 * @brief 音频文件转写页面
 *
 * 支持单文件/批量转写，进度显示，结果导出。
 * 解码和推理在后台线程执行，不阻塞 UI。
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
    void onTaskComplete(int index, const QString& text, bool success,
                        double durationSec, int sampleRate, int channels);
    void onAllComplete();

private:
    void setupUI();
    void updateUIState();
    void startBatchTranscription();
    void processFileAsync(int index);

    // 导出辅助方法
    QString exportTXT(const QList<TranscribeTask>& tasks) const;
    QString exportSRT(const QList<TranscribeTask>& tasks) const;
    QByteArray exportJSON(const QList<TranscribeTask>& tasks) const;
    QString formatSRTTime(double seconds) const;

    ConfigManager* configManager_;
    SenseVoiceEngine* sttEngine_;
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
    int activeWorkers_ = 0; // 正在处理的任务数
};

} // namespace impress
