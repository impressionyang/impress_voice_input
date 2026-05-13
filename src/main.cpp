#include "app/application.h"
#include "ui/main_window.h"
#include "app/config_manager.h"
#include "utils/logger.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCommandLineParser>

int main(int argc, char* argv[])
{
    impress::Application app(argc, argv);
    app.setApplicationName("Impress Voice Input");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("Impress");

    // 初始化日志文件
    QString logDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    QString logFilePath = logDir + "/app.log";
    impress::Logger::init(logFilePath);

    LOG_INFO("Main", QString("应用启动，日志文件: %1").arg(logFilePath));

    // 命令行参数
    QCommandLineParser parser;
    parser.setApplicationDescription("基于 ONNX 的实时语音转文本输入法");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {{"c", "config"}, "指定配置文件路径", "path"},
        {{"m", "model"}, "指定模型路径", "path"},
    });
    parser.process(app);

    // 加载用户配置
    auto* configManager = app.configManager();
    QString configPath = parser.value("config");
    if (configPath.isEmpty()) {
        // 使用默认配置目录
        configPath = logDir + "/config.json";
    }

    if (QFile::exists(configPath)) {
        configManager->load(configPath);
        LOG_INFO("Main", QString("已加载配置: %1").arg(configPath));
    } else {
        LOG_INFO("Main", "使用默认配置");
    }

    // 命令行覆盖模型路径
    QString modelPath = parser.value("model");
    if (!modelPath.isEmpty()) {
        configManager->set("stt.model_path", modelPath);
    }

    // 创建并显示主窗口（传入全局引擎）
    impress::MainWindow mainWindow(configManager, app.sttEngine());
    mainWindow.show();

    return app.exec();
}
