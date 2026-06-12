#include "main_window.h"
#include "stt_test_page.h"
#include "file_transcribe_page.h"
#include "settings_page.h"
#include "core/voice_input_service.h"
#include "core/sense_voice_engine.h"
#include "app/config_manager.h"
#include "app/application.h"
#include "utils/logger.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFile>
#include <QMessageBox>
#include <QStatusBar>
#include <QLabel>
#include <QFileInfo>
#include <QSystemTrayIcon>
#include <QApplication>
#include <QCloseEvent>
#include <QStyle>
#include <QIcon>
#include <QTimer>
#include <QTabBar>
#ifdef Q_OS_WIN
#include <windows.h>

// 枚举并隐藏 Qt 创建的多余工具窗口（无标题栏、无边框、非主窗口的 WS_EX_TOOLWINDOW）
static BOOL CALLBACK HideQtToolWindows(HWND hwnd, LPARAM /*lParam*/) {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    LONG style = GetWindowLong(hwnd, GWL_STYLE);

    // 只找 WS_EX_TOOLWINDOW 的窗口
    if ((exStyle & WS_EX_TOOLWINDOW) == 0) return TRUE;

    // 排除有标题栏的窗口
    bool hasTitleBar = (style & WS_CAPTION) != 0;
    if (hasTitleBar) return TRUE;

    // 排除子窗口
    if (style & WS_CHILD) return TRUE;

    // 获取窗口标题
    wchar_t title[256];
    int len = GetWindowTextW(hwnd, title, 256);

    // 排除 Qt 标题栏窗口（_q_titlebar）
    if (len > 0 && QString::fromWCharArray(title, len).startsWith("_q_")) return TRUE;

    // 找到了可疑窗口，隐藏它
    RECT rect;
    GetWindowRect(hwnd, &rect);
    LOG_INFO("MainWindow", QString("发现并隐藏 Qt 内部工具窗口: HWND=%1 标题=\"%2\" 大小=%3x%4")
        .arg((qulonglong)hwnd)
        .arg(len > 0 ? QString::fromWCharArray(title, len) : "(无标题)")
        .arg(rect.right - rect.left)
        .arg(rect.bottom - rect.top));

    ShowWindow(hwnd, SW_HIDE);
    return TRUE;
}
#endif

static const char* const kTag = "MainWindow";

namespace impress {

MainWindow::MainWindow(ConfigManager* configManager,
                       SenseVoiceEngine* sttEngine,
                       QWidget* parent)
    : QMainWindow(parent)
    , configManager_(configManager)
{
    LOG_INFO(kTag, "MainWindow 构造函数开始");

    setWindowTitle("Impress Voice Input");
    resize(1000, 700);

    // 设置窗口图标
    setWindowIcon(QIcon(":/icons/app_icon.png"));
    LOG_INFO(kTag, "窗口图标已设置");

    LOG_INFO(kTag, "开始 setupUI");
    setupUI(sttEngine);
    LOG_INFO(kTag, "setupUI 完成");

    LOG_INFO(kTag, "开始 setupMenuBar");
    setupMenuBar();
    LOG_INFO(kTag, "setupMenuBar 完成");

    LOG_INFO(kTag, "开始 setupStatusBar");
    setupStatusBar(sttEngine);
    LOG_INFO(kTag, "setupStatusBar 完成");

    LOG_INFO(kTag, "开始 setupTrayIcon");
    setupTrayIcon();
    LOG_INFO(kTag, "setupTrayIcon 完成");

    // 初始化语音输入服务（共享全局引擎）
    LOG_INFO(kTag, "开始创建 VoiceInputService");
    voiceInputService_ = new VoiceInputService(configManager_, sttEngine, this);
    connect(voiceInputService_, &VoiceInputService::statusChanged,
            this, [this](const QString& status) {
                LOG_DEBUG(kTag, QString("语音输入状态: %1").arg(status));
                updateTrayIcon(status);
            });
    connect(voiceInputService_, &VoiceInputService::error,
            this, [this](const QString& err) {
                LOG_ERROR(kTag, err);
            });
    connect(voiceInputService_, &VoiceInputService::recognitionResult,
            this, [this](const QString& text) {
                LOG_INFO(kTag, QString("语音识别结果: %1").arg(text));
            });
    LOG_INFO(kTag, "VoiceInputService 已创建");

    // 监听配置变化，动态启停语音输入服务
    connect(configManager_, &ConfigManager::configChanged,
            this, &MainWindow::onVoiceInputConfigChanged);

    // 启动时检查配置
    LOG_INFO(kTag, "开始 onVoiceInputConfigChanged");
    onVoiceInputConfigChanged();
    LOG_INFO(kTag, "onVoiceInputConfigChanged 完成");

#ifdef Q_OS_WIN
    // 延迟隐藏 Qt 在 Windows 上创建的额外工具窗口
    QTimer::singleShot(500, this, []() {
        LOG_INFO(kTag, "开始检查 Qt 内部工具窗口");
        EnumWindows(HideQtToolWindows, 0);
        LOG_INFO(kTag, "Qt 内部工具窗口检查完成");
    });
#endif

    LOG_INFO(kTag, "主窗口已创建");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI(SenseVoiceEngine* sttEngine) {
    LOG_INFO(kTag, "setupUI: 创建 QTabWidget");
    tabWidget_ = new QTabWidget(this);

    // 禁用可能导致额外窗口的功能
    tabWidget_->setDocumentMode(true);
    tabWidget_->setTabBarAutoHide(false);
    tabWidget_->tabBar()->setExpanding(true);
    tabWidget_->tabBar()->setMovable(false);
    tabWidget_->tabBar()->setDrawBase(true);

    LOG_INFO(kTag, "setupUI: 创建 STTTestPage");
    sttPage_ = new STTTestPage(configManager_, sttEngine, tabWidget_);
    LOG_INFO(kTag, "setupUI: STTTestPage 创建完成");

    LOG_INFO(kTag, "setupUI: 创建 FileTranscribePage");
    transcribePage_ = new FileTranscribePage(configManager_, sttEngine, tabWidget_);
    LOG_INFO(kTag, "setupUI: FileTranscribePage 创建完成");

    LOG_INFO(kTag, "setupUI: 创建 SettingsPage");
    settingsPage_ = new SettingsPage(configManager_, tabWidget_);
    LOG_INFO(kTag, "setupUI: SettingsPage 创建完成");

    tabWidget_->addTab(sttPage_, "实时语音识别");
    tabWidget_->addTab(transcribePage_, "音频文件转写");
    tabWidget_->addTab(settingsPage_, "配置");
    LOG_INFO(kTag, "setupUI: Tab 页面已添加");

    setCentralWidget(tabWidget_);
    LOG_INFO(kTag, "setupUI: 完成");
}

void MainWindow::setupStatusBar(SenseVoiceEngine* sttEngine) {
    sttEngine_ = sttEngine;

    auto* bar = statusBar();
    bar->setSizeGripEnabled(false);

    modelStatusLabel_ = new QLabel(this);
    modelStatusLabel_->setStyleSheet(
        "QLabel { padding: 2px 8px; font-size: 13px; }");
    bar->addPermanentWidget(modelStatusLabel_);

    // 连接全局引擎信号
    connect(sttEngine_, &SenseVoiceEngine::modelLoaded,
            this, &MainWindow::updateModelStatus);
    connect(sttEngine_, &SenseVoiceEngine::modelLoadError,
            this, &MainWindow::updateModelStatus);
    connect(sttEngine_, &SenseVoiceEngine::modelUnloaded,
            this, &MainWindow::updateModelStatus);

    // 初始化显示
    updateModelStatus();
}

void MainWindow::setupTrayIcon() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        LOG_INFO(kTag, "系统托盘不可用");
        return;
    }

    trayMenu_ = new QMenu(this);
    auto* showAction = trayMenu_->addAction("显示主窗口");
    connect(showAction, &QAction::triggered, this, [this]() {
        showNormal();
        activateWindow();
        raise();
    });
    trayMenu_->addSeparator();
    auto* exitAction = trayMenu_->addAction("退出");
    connect(exitAction, &QAction::triggered, this, [this]() {
        doExit();
    });

    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setContextMenu(trayMenu_);

    // 默认状态：根据主题颜色生成图标
    idleIcon_ = Application::createTrayIcon(false);
    activeIcon_ = Application::createTrayIcon(true);

    trayIcon_->setIcon(idleIcon_);
    trayIcon_->setToolTip("Impress Voice Input - 语音输入就绪");
    trayIcon_->show();

    // 双击托盘显示窗口
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            showNormal();
            activateWindow();
            raise();
        }
    });

    LOG_INFO(kTag, "系统托盘图标已创建");
}

void MainWindow::updateTrayIcon(const QString& status) {
    if (!trayIcon_) return;

    // 录音/识别 → 播放图标，就绪/停止 → 停止图标
    if (status.contains("正在录音") || status.contains("正在识别")) {
        trayIcon_->setIcon(activeIcon_);
    } else {
        trayIcon_->setIcon(idleIcon_);
    }
    trayIcon_->setToolTip(QString("Impress Voice Input - %1").arg(status));
}

void MainWindow::setupMenuBar() {
    // 文件菜单
    auto* fileMenu = menuBar()->addMenu("文件");

    auto* exportAction = fileMenu->addAction("导出结果");
    exportAction->setShortcut(QKeySequence("Ctrl+E"));

    fileMenu->addSeparator();

    auto* exitAction = fileMenu->addAction("退出");
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction, &QAction::triggered, this, [this]() {
        doExit();
    });

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
    // 关闭按钮隐藏到托盘，不退出程序
    LOG_INFO(kTag, "主窗口隐藏到托盘");
    hide();
    event->ignore();
}

void MainWindow::doExit() {
    LOG_INFO(kTag, "应用退出");
    if (voiceInputService_) {
        voiceInputService_->stop();
    }
    if (trayIcon_) {
        trayIcon_->hide();
    }
    qApp->quit();
}

void MainWindow::updateModelStatus() {
    if (!sttEngine_ || !modelStatusLabel_) return;

    QString modelPath = configManager_->get("stt.model_path").toString();
    QString modelName = modelPath.isEmpty() ? QString() : QFileInfo(modelPath).fileName();

    if (sttEngine_->isLoaded()) {
        QString statusText = modelName.isEmpty() ? "模型已就绪" : QString("模型已就绪: %1").arg(modelName);
        modelStatusLabel_->setText(statusText);
        modelStatusLabel_->setStyleSheet(
            "QLabel { padding: 2px 8px; font-size: 13px; color: #27ae60; font-weight: bold; }");
    } else if (modelPath.isEmpty()) {
        modelStatusLabel_->setText("⚠️ 模型路径未设置");
        modelStatusLabel_->setStyleSheet(
            "QLabel { padding: 2px 8px; font-size: 13px; color: #e74c3c; }");
    } else {
        modelStatusLabel_->setText(
            QString("⚠️ 模型加载失败: %1").arg(modelName));
        modelStatusLabel_->setStyleSheet(
            "QLabel { padding: 2px 8px; font-size: 13px; color: #e67e22; }");
    }
}

void MainWindow::onVoiceInputConfigChanged() {
    if (!voiceInputService_) return;

    // 动态应用主题和字体
    QString theme = configManager_->get("ui.theme").toString();
    int fontSize = configManager_->get("ui.font_size").toInt();
    Application::applyTheme(theme);
    if (fontSize > 0) Application::applyFontSize(fontSize);

    // 刷新托盘图标颜色
    idleIcon_ = Application::createTrayIcon(false);
    activeIcon_ = Application::createTrayIcon(true);
    updateTrayIcon("语音输入就绪");

    // 更新模型状态显示
    updateModelStatus();

    // 当设置了语音快捷键时启用语音输入服务
    QString hotkey = configManager_->get("shortcuts.voice_hotkey").toString();
    bool enabled = !hotkey.isEmpty() && hotkey != "未设置";
    if (enabled && !voiceInputService_->isRunning()) {
        voiceInputService_->start();
        LOG_INFO(kTag, QString("语音输入已启用（快捷键: %1）").arg(hotkey));
    } else if (!enabled && voiceInputService_->isRunning()) {
        voiceInputService_->stop();
        LOG_INFO(kTag, "语音输入已关闭");
    }
}

} // namespace impress
