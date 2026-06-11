#include "app/application.h"
#include "ui/main_window.h"
#include "app/config_manager.h"
#include "utils/logger.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCommandLineParser>
#include <QApplication>

int main(int argc, char* argv[])
{
    impress::Application app(argc, argv);
    app.setApplicationName("Impress Voice Input");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("Impress");

    // 默认日志目录（配置加载后可能被覆盖）
    QString defaultLogDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(defaultLogDir);

    // 命令行参数
    QCommandLineParser parser;
    parser.setApplicationDescription("基于 ONNX 的实时语音转文本输入法");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {{"c", "config"}, "指定配置文件路径", "path"},
        {{"m", "model"}, "指定模型路径", "path"},
        {{"l", "log-dir"}, "指定日志文件目录", "path"},
    });
    parser.process(app);

    // 加载用户配置
    auto* configManager = app.configManager();
    QString configPath = parser.value("config");
    if (configPath.isEmpty()) {
        configPath = defaultLogDir + "/config.json";
    }

    if (QFile::exists(configPath)) {
        configManager->load(configPath);
        LOG_INFO("Main", QString("已加载配置: %1").arg(configPath));
    } else {
        LOG_INFO("Main", "使用默认配置");
    }

    // 确定日志目录：命令行 > 配置 > 默认 AppDataLocation
    QString effectiveLogDir = parser.value("log-dir");
    if (effectiveLogDir.isEmpty()) {
        effectiveLogDir = configManager->get("app.log_dir").toString();
    }
    if (effectiveLogDir.isEmpty()) {
        effectiveLogDir = defaultLogDir;
    }
    QDir().mkpath(effectiveLogDir);
    QString logFilePath = effectiveLogDir + "/app.log";
    impress::Logger::init(logFilePath);

    LOG_INFO("Main", QString("日志目录: %1").arg(effectiveLogDir));

    // 命令行覆盖模型路径
    QString modelPath = parser.value("model");
    if (!modelPath.isEmpty()) {
        configManager->set("stt.model_path", modelPath);
    }

    // 应用主题和字体
    QString theme = configManager->get("ui.theme").toString();
    int fontSize = configManager->get("ui.font_size").toInt();
    impress::Application::applyTheme(theme);
    if (fontSize > 0) impress::Application::applyFontSize(fontSize);
    LOG_INFO("Main", QString("主题: %1, 字体: %2").arg(theme).arg(fontSize));

    // 配置加载完成后，启动全局模型加载
    app.loadGlobalModel();

    // 创建并显示主窗口（传入全局引擎）
    impress::MainWindow mainWindow(configManager, app.sttEngine());
    mainWindow.show();

    return app.exec();
}
