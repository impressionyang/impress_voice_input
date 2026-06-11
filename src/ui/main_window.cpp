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
#include <QStatusBar>
#include <QLabel>
#include <QFileInfo>
#include <QSystemTrayIcon>
#include <QPainter>
#include <QIcon>
#include <QApplication>
#include <QCloseEvent>

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
    setupStatusBar(sttEngine);
    setupTrayIcon();
    loadStyleSheet();

    // 初始化语音输入服务（共享全局引擎）
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

    // 防止关闭主窗口时应用退出，保持托盘图标运行
    qApp->setQuitOnLastWindowClosed(false);

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
        if (voiceInputService_) {
            voiceInputService_->stop();
        }
        qApp->quit();
    });

    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setContextMenu(trayMenu_);

    // 先设置图标，再显示托盘
    updateTrayIcon("语音输入就绪");
    trayIcon_->show();

    // 双击托盘显示窗口
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            showNormal();
            activateWindow();
            raise();
        }
    });

    // 发送通知气泡，让用户注意到托盘图标
    trayIcon_->showMessage(
        "Impress Voice Input",
        "语音输入已就绪，CapsLock 快捷键已注册",
        QSystemTrayIcon::Information,
        3000
    );

    LOG_INFO(kTag, QString("系统托盘图标已创建 (可用: %1, 已设置 quitOnLastWindowClosed=false)")
        .arg(QSystemTrayIcon::isSystemTrayAvailable()));
}

void MainWindow::updateTrayIcon(const QString& status) {
    if (!trayIcon_) return;

    QColor color;

    // 根据状态文字匹配图标颜色
    if (status.contains("正在录音")) {
        color = QColor("#e74c3c");  // 红色 - 录音中
    } else if (status.contains("正在识别")) {
        color = QColor("#f39c12");  // 橙色 - 识别中
    } else if (status.contains("等待长按") || status.contains("PreRecording")) {
        color = QColor("#f1c40f");  // 黄色 - 预录音
    } else if (status.contains("已启动") || status.contains("就绪")) {
        color = QColor("#27ae60");  // 绿色 - 就绪
    } else if (status.contains("已关闭") || status.contains("停止")) {
        color = QColor("#95a5a6");  // 灰色 - 停止
    } else {
        color = QColor("#3498db");  // 蓝色 - 其他
    }

    QPixmap pm = createTrayIcon(color);
    QIcon icon(pm);
    if (icon.isNull()) {
        LOG_ERROR(kTag, "托盘图标创建失败");
        return;
    }

    trayIcon_->setIcon(icon);
    trayIcon_->setToolTip(QString("Impress Voice Input - %1").arg(status));
}

QPixmap MainWindow::createTrayIcon(const QColor& color) {
    // 使用 16x16 标准托盘图标尺寸
    const int size = 16;
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制实心彩色圆
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    int margin = 1;
    painter.drawEllipse(margin, margin, size - 2 * margin, size - 2 * margin);

    return QPixmap::fromImage(image);
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
        if (trayIcon_) {
            trayIcon_->hide();
        }
        if (voiceInputService_) {
            voiceInputService_->stop();
        }
        qApp->quit();
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
    if (trayIcon_) {
        trayIcon_->hide();
    }
    if (voiceInputService_) {
        voiceInputService_->stop();
    }
    LOG_INFO(kTag, "主窗口关闭 (应用保持运行，托盘图标可见)");
    event->ignore();  // 不关闭应用，只隐藏窗口
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
