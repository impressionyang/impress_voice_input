#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <memory>

namespace impress {

class ConfigManager;
class SenseVoiceEngine;
class STTTestPage;
class FileTranscribePage;
class SettingsPage;
class VoiceInputService;

/**
 * @brief 主窗口
 *
 * 使用 Tab 页导航管理三个功能页面，共享全局 STT 引擎。
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
    void loadStyleSheet();
    void onVoiceInputConfigChanged();

    ConfigManager* configManager_;
    VoiceInputService* voiceInputService_;
    STTTestPage* sttPage_;
    FileTranscribePage* transcribePage_;
    SettingsPage* settingsPage_;
    QTabWidget* tabWidget_;
};

} // namespace impress
