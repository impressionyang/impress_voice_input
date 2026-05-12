#include "main_window.h"
#include "stt_test_page.h"
#include "file_transcribe_page.h"
#include "settings_page.h"
#include "app/config_manager.h"
#include "utils/logger.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFile>
#include <QMessageBox>

static const char* const kTag = "MainWindow";

namespace impress {

MainWindow::MainWindow(ConfigManager* configManager, QWidget* parent)
    : QMainWindow(parent)
    , configManager_(configManager)
{
    setWindowTitle("Impress Voice Input");
    resize(1000, 700);

    setupUI();
    setupMenuBar();
    loadStyleSheet();

    LOG_INFO(kTag, "主窗口已创建");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    tabWidget_ = new QTabWidget(this);

    sttPage_ = new STTTestPage(configManager_, tabWidget_);
    transcribePage_ = new FileTranscribePage(configManager_, tabWidget_);
    settingsPage_ = new SettingsPage(configManager_, tabWidget_);

    tabWidget_->addTab(sttPage_, "实时语音识别");
    tabWidget_->addTab(transcribePage_, "音频文件转写");
    tabWidget_->addTab(settingsPage_, "配置");

    setCentralWidget(tabWidget_);
}

void MainWindow::setupMenuBar() {
    // 文件菜单
    auto* fileMenu = menuBar()->addMenu("文件");

    auto* exportAction = fileMenu->addAction("导出结果");
    exportAction->setShortcut(QKeySequence("Ctrl+E"));

    fileMenu->addSeparator();

    auto* exitAction = fileMenu->addAction("退出");
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction, &QAction::triggered, this, &MainWindow::close);

    // 帮助菜单
    auto* helpMenu = menuBar()->addMenu("帮助");

    auto* aboutAction = helpMenu->addAction("关于");
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(this, "关于",
            "<h2>Impress Voice Input</h2>"
            "<p>基于 ONNX 的实时语音转文本输入法</p>"
            "<p>版本: 0.1.0</p>");
    });
}

void MainWindow::loadStyleSheet() {
    QFile styleFile(":/styles/main.qss");
    if (styleFile.open(QIODevice::ReadOnly)) {
        setStyleSheet(styleFile.readAll());
        styleFile.close();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    LOG_INFO(kTag, "主窗口关闭");
    QMainWindow::closeEvent(event);
}

} // namespace impress
