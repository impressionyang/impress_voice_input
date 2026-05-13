#include "main_window.h"
#include "stt_test_page.h"
#include "file_transcribe_page.h"
#include "settings_page.h"
#include "core/voice_input_service.h"
#include "core/sense_voice_engine.h"
#include "app/config_manager.h"
#include "utils/logger.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFile>
#include <QMessageBox>

static const char* const kTag = "MainWindow";

namespace impress {

MainWindow::MainWindow(ConfigManager* configManager,
                       SenseVoiceEngine* sttEngine,
                       QWidget* parent)
    : QMainWindow(parent)
    , configManager_(configManager)
{
    setWindowTitle("Impress Voice Input");
    resize(1000, 700);

    setupUI(sttEngine);
    setupMenuBar();
    loadStyleSheet();

    // 初始化语音输入服务（共享全局引擎）
    voiceInputService_ = new VoiceInputService(configManager_, sttEngine, this);
    connect(voiceInputService_, &VoiceInputService::statusChanged,
            this, [this](const QString& status) {
                LOG_DEBUG(kTag, QString("语音输入状态: %1").arg(status));
            });
    connect(voiceInputService_, &VoiceInputService::error,
            this, [this](const QString& err) {
                LOG_ERROR(kTag, err);
            });
    connect(voiceInputService_, &VoiceInputService::recognitionResult,
            this, [this](const QString& text) {
                LOG_INFO(kTag, QString("语音识别结果: %1").arg(text));
            });

    // 监听配置变化，动态启停语音输入服务
    connect(configManager_, &ConfigManager::configChanged,
            this, &MainWindow::onVoiceInputConfigChanged);

    // 启动时检查配置
    onVoiceInputConfigChanged();

    LOG_INFO(kTag, "主窗口已创建");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI(SenseVoiceEngine* sttEngine) {
    tabWidget_ = new QTabWidget(this);

    sttPage_ = new STTTestPage(configManager_, sttEngine, tabWidget_);
    transcribePage_ = new FileTranscribePage(configManager_, sttEngine, tabWidget_);
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
    if (voiceInputService_) {
        voiceInputService_->stop();
    }
    LOG_INFO(kTag, "主窗口关闭");
    QMainWindow::closeEvent(event);
}

void MainWindow::onVoiceInputConfigChanged() {
    if (!voiceInputService_) return;

    bool enabled = configManager_->get("stt.capslock_voice_enabled").toBool();
    if (enabled && !voiceInputService_->isRunning()) {
        voiceInputService_->start();
        LOG_INFO(kTag, "CapsLock 语音输入已启用");
    } else if (!enabled && voiceInputService_->isRunning()) {
        voiceInputService_->stop();
        LOG_INFO(kTag, "CapsLock 语音输入已关闭");
    }
}

} // namespace impress
