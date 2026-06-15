#include "app/application.h"
#include "ui/main_window.h"
#include "app/config_manager.h"
#include "utils/logger.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCommandLineParser>
#include <QApplication>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <string>

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;

    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);

    RECT rect;
    GetWindowRect(hwnd, &rect);

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    bool hasTitleBar = (style & WS_CAPTION) != 0;
    bool hasBorder = (style & WS_BORDER) != 0;
    bool isChild = (style & WS_CHILD) != 0;
    bool isVisible = IsWindowVisible(hwnd);
    bool isTool = (exStyle & WS_EX_TOOLWINDOW) != 0;

    std::wstring wTitle(title);
    std::string utf8Title(wTitle.begin(), wTitle.end());

    LOG_INFO("WindowEnum", QString("HWND=%1 标题=\"%2\" 位置=[%3,%4,%5,%6] 大小=%7x%8 "
        "可见=%9 标题栏=%10 边框=%11 子窗口=%12 工具窗=%13")
        .arg((qulonglong)hwnd)
        .arg(QString::fromStdWString(wTitle))
        .arg(rect.left).arg(rect.top).arg(rect.right).arg(rect.bottom)
        .arg(rect.right - rect.left).arg(rect.bottom - rect.top)
        .arg(isVisible ? "Y" : "N")
        .arg(hasTitleBar ? "Y" : "N")
        .arg(hasBorder ? "Y" : "N")
        .arg(isChild ? "Y" : "N")
        .arg(isTool ? "Y" : "N"));

    return TRUE;
}

static void dumpAllWindows() {
    LOG_INFO("WindowEnum", "===== 枚举进程所有窗口 =====");
    EnumWindows(EnumWindowsProc, 0);
    LOG_INFO("WindowEnum", "===== 窗口枚举结束 =====");
}
#endif

int main(int argc, char* argv[])
{
    impress::Application app(argc, argv);
    app.setApplicationName("Impress Voice Input");
    app.setApplicationVersion("0.1.1");
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

    LOG_INFO("Main", QString("=== Impress Voice Input v%1 ===").arg(app.applicationVersion()));
    LOG_INFO("Main", QString("编译时间: %1 %2").arg(__DATE__).arg(__TIME__));
    LOG_INFO("Main", QString("Qt 版本: %1").arg(qVersion()));
#ifdef Q_OS_WIN
    LOG_INFO("Main", "平台: Windows");
#elif defined(Q_OS_LINUX)
    LOG_INFO("Main", "平台: Linux");
#endif

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

#ifdef Q_OS_WIN
    // 延迟 1 秒后枚举所有窗口，确保所有 Qt 内部窗口都已创建
    QTimer::singleShot(1000, []() {
        dumpAllWindows();
    });

    // 3 秒后再次枚举，检测是否有延迟创建的窗口
    QTimer::singleShot(3000, []() {
        LOG_INFO("WindowEnum", "===== 延迟 3 秒后再次枚举 =====");
        dumpAllWindows();
    });
#endif

    return app.exec();
}
