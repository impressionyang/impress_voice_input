#pragma once

#include <QCoreApplication>
#include <QString>
#include <QMutex>
#include <QFile>

namespace impress {

/**
 * @brief 日志级别
 */
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

/**
 * @brief 全局日志工具
 *
 * 同时输出到控制台和文件。线程安全。
 */
class Logger {
public:
    /** @brief 初始化日志（可选指定日志文件路径） */
    static void init(const QString& logFilePath = QString());

    /** @brief 关闭日志文件 */
    static void shutdown();

    static void log(LogLevel level, const QString& tag, const QString& message);
    static void debug(const QString& tag, const QString& message);
    static void info(const QString& tag, const QString& message);
    static void warning(const QString& tag, const QString& message);
    static void error(const QString& tag, const QString& message);

    /** @brief 设置日志文件路径（运行时切换） */
    static void setLogFile(const QString& path);

private:
    static QString levelToString(LogLevel level);
    static QString getTimestamp();
    static void writeToFile(const QString& line);

    static QMutex mutex_;
    static QFile* logFile_;
};

// 便捷宏
#define LOG_DEBUG(tag, msg)  ::impress::Logger::debug(tag, msg)
#define LOG_INFO(tag, msg)   ::impress::Logger::info(tag, msg)
#define LOG_WARNING(tag, msg) ::impress::Logger::warning(tag, msg)
#define LOG_ERROR(tag, msg)  ::impress::Logger::error(tag, msg)

} // namespace impress
