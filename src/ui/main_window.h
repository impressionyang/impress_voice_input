#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <memory>

namespace impress {

class ConfigManager;
class STTTestPage;
class FileTranscribePage;
class SettingsPage;

/**
 * @brief 主窗口
 *
 * 使用 Tab 页导航管理三个功能页面。
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ConfigManager* configManager, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUI();
    void setupMenuBar();
    void loadStyleSheet();

    ConfigManager* configManager_;
    STTTestPage* sttPage_;
    FileTranscribePage* transcribePage_;
    SettingsPage* settingsPage_;
    QTabWidget* tabWidget_;
};

} // namespace impress
