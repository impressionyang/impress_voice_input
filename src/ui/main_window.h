#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <memory>

class QLabel;
class QSystemTrayIcon;
class QMenu;
class QIcon;

namespace impress {

class ConfigManager;
class SenseVoiceEngine;
class STTTestPage;
class FileTranscribePage;
class SettingsPage;
class VoiceInputService;
class RecordingIndicator;

/**
 * @brief 主窗口
 *
 * 使用 Tab 页导航管理三个功能页面，共享全局 STT 引擎。
 * 状态栏显示模型名称和加载状态。
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ConfigManager* configManager,
                        SenseVoiceEngine* sttEngine,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUI(SenseVoiceEngine* sttEngine);
    void setupMenuBar();
    void setupStatusBar(SenseVoiceEngine* sttEngine);
    void setupTrayIcon();
    void updateTrayIcon(const QString& status);
    void loadStyleSheet();
    void onVoiceInputConfigChanged();
    void updateModelStatus();
    void doExit();
    void doRestart();
    void showUsage();

    ConfigManager* configManager_;
    SenseVoiceEngine* sttEngine_;
    VoiceInputService* voiceInputService_;
    STTTestPage* sttPage_;
    FileTranscribePage* transcribePage_;
    SettingsPage* settingsPage_;
    QTabWidget* tabWidget_;
    QLabel* modelStatusLabel_;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    QIcon idleIcon_;   // SP_MediaStop — 就绪/停止
    QIcon activeIcon_; // SP_MediaPlay — 录音/识别
    RecordingIndicator* recordingIndicator_ = nullptr;
};

} // namespace impress
