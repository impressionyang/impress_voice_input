#include "application.h"
#include "config_manager.h"
#include "core/sense_voice_engine.h"
#include "utils/logger.h"
#include <QFile>
#include <QPalette>
#include <QColor>
#include <QStyleFactory>
#include <QStyle>
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QPainter>

namespace impress {

static QString s_currentTheme; // 跟踪当前主题

static const char* const kTag = "Application";

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    LOG_INFO(kTag, QString("Impress Voice Input v%1 启动").arg(applicationVersion()));
    LOG_INFO(kTag, QString("编译时间: %1 %2").arg(__DATE__).arg(__TIME__));

    configManager_ = std::make_unique<ConfigManager>(this);
    configManager_->loadDefaults();

    // 创建全局 STT 引擎（共享实例）
    sttEngine_ = new SenseVoiceEngine(this);
    connect(sttEngine_, &SenseVoiceEngine::modelLoaded, this, [this](const QString& path) {
        modelLoaded_ = true;
        modelPath_ = path;
        LOG_INFO(kTag, QString("全局模型已加载: %1").arg(path));
        emit modelLoaded(path);
    });
    connect(sttEngine_, &SenseVoiceEngine::modelLoadError, this, [this](const QString&, const QString& err) {
        modelLoaded_ = false;
        LOG_ERROR(kTag, QString("全局模型加载失败: %1").arg(err));
        emit modelLoadError(err);
    });
}

Application::~Application() {
    LOG_INFO(kTag, "应用退出");
    Logger::shutdown();
}

ConfigManager* Application::configManager() const {
    return configManager_.get();
}

SenseVoiceEngine* Application::sttEngine() const {
    return sttEngine_;
}

bool Application::isModelLoaded() const {
    return modelLoaded_;
}

void Application::loadGlobalModel() {
    QString modelPath = configManager_->get("stt.model_path").toString();
    if (modelPath.isEmpty()) {
        LOG_WARNING(kTag, "模型路径为空，请在配置中设置后重启");
        emit modelLoadError("模型路径未设置");
        return;
    }

    modelPath_ = modelPath;

    QString tokensPath = configManager_->get("stt.tokens_path").toString();
    QString device = configManager_->get("stt.device").toString();
    int numThreads = configManager_->get("stt.num_threads").toInt();

    bool debugSave = configManager_->get("stt.debug_save_audio").toBool();
    sttEngine_->setDebugSaveAudio(debugSave);

    LOG_INFO(kTag, QString("正在异步加载全局模型: %1").arg(modelPath));
    emit modelLoading(modelPath);
    sttEngine_->loadModelAsync(modelPath, tokensPath, device, numThreads);
}

void Application::applyTheme(const QString& theme) {
    s_currentTheme = theme;

    // 1. 先设置风格（必须在 palette 和 stylesheet 之前）
    qApp->setStyle(QStyleFactory::create("Fusion"));

    // 2. 设置调色板
    QPalette palette;
    if (theme == "dark") {
        palette.setColor(QPalette::Window, QColor(53, 53, 53));
        palette.setColor(QPalette::WindowText, Qt::white);
        palette.setColor(QPalette::Base, QColor(25, 25, 25));
        palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        palette.setColor(QPalette::ToolTipBase, Qt::white);
        palette.setColor(QPalette::ToolTipText, Qt::white);
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::Button, QColor(53, 53, 53));
        palette.setColor(QPalette::ButtonText, Qt::white);
        palette.setColor(QPalette::BrightText, Qt::cyan);
        palette.setColor(QPalette::Link, QColor(42, 130, 218));
        palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        palette.setColor(QPalette::HighlightedText, Qt::black);
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
        palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
        palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
    } else {
        // 显式设置亮色主题（不依赖 standardPalette，兼容 Fusion 风格）
        palette.setColor(QPalette::Window, QColor(255, 255, 255));
        palette.setColor(QPalette::WindowText, QColor(34, 34, 34));
        palette.setColor(QPalette::Base, QColor(255, 255, 255));
        palette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
        palette.setColor(QPalette::ToolTipBase, Qt::white);
        palette.setColor(QPalette::ToolTipText, QColor(34, 34, 34));
        palette.setColor(QPalette::Text, QColor(34, 34, 34));
        palette.setColor(QPalette::Button, QColor(255, 255, 255));
        palette.setColor(QPalette::ButtonText, QColor(34, 34, 34));
        palette.setColor(QPalette::BrightText, QColor(0, 150, 136));
        palette.setColor(QPalette::Link, QColor(42, 130, 218));
        palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(180, 180, 180));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(180, 180, 180));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(180, 180, 180));
        palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(200, 200, 200));
        palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(160, 160, 160));
    }
    qApp->setPalette(palette);

    // 3. 最后设置样式表（覆盖 palette）
    const QString qssPath = (theme == "dark") ? ":/styles/main_dark.qss" : ":/styles/main.qss";
    QFile styleFile(qssPath);
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet(styleFile.readAll());
        styleFile.close();
    } else {
        LOG_ERROR("Theme", QString("无法加载样式表: %1").arg(qssPath));
    }

    LOG_INFO("Theme", QString("主题已切换: %1").arg(theme));
}

void Application::applyFontSize(int size) {
    QFont font = qApp->font();
    font.setPointSize(size);
    qApp->setFont(font);
    LOG_INFO("Theme", QString("字体大小已设置: %1").arg(size));
}

QIcon Application::createTrayIcon(bool active) {
    const QColor color = (s_currentTheme == "dark") ? Qt::white : Qt::black;
    const int size = 16;

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    if (active) {
        // 播放图标（三角形）
        const int margin = 3;
        QPolygon triangle;
        triangle << QPoint(margin, margin)
                 << QPoint(margin, size - margin)
                 << QPoint(size - margin, size / 2);
        painter.drawPolygon(triangle);
    } else {
        // 停止图标（正方形）
        const int margin = 3;
        painter.drawRect(margin, margin, size - 2 * margin, size - 2 * margin);
    }

    return QIcon(pixmap);
}

bool Application::isDarkTheme() {
    return s_currentTheme == "dark";
}

} // namespace impress
