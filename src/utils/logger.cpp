#include "logger.h"
#include <QDateTime>
#include <QDebug>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <iostream>

namespace impress {

QMutex Logger::mutex_;
QFile* Logger::logFile_ = nullptr;

void Logger::init(const QString& logFilePath) {
    QMutexLocker locker(&mutex_);
    qSetMessagePattern("[%{time yyyy-MM-dd hh:mm:ss.zzz}] %{message}");

    // 确定日志文件路径
    QString path = logFilePath;
    if (path.isEmpty()) {
        QString logDir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        QDir().mkpath(logDir);
        path = logDir + "/app.log";
    }

    logFile_ = new QFile(path);
    if (logFile_->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // Qt 6 没有 setAutoFlush，每次 write 后手动 flush
    } else {
        delete logFile_;
        logFile_ = nullptr;
        std::cerr << "[Logger] 无法打开日志文件: " << path.toStdString() << std::endl;
    }
}

void Logger::shutdown() {
    QMutexLocker locker(&mutex_);
    if (logFile_) {
        logFile_->flush();
        logFile_->close();
        delete logFile_;
        logFile_ = nullptr;
    }
}

void Logger::setLogFile(const QString& path) {
    QMutexLocker locker(&mutex_);
    if (logFile_) {
        logFile_->flush();
        logFile_->close();
        delete logFile_;
    }
    logFile_ = new QFile(path);
    if (logFile_->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // Qt 6 没有 setAutoFlush，每次 write 后手动 flush
    } else {
        delete logFile_;
        logFile_ = nullptr;
    }
}

void Logger::log(LogLevel level, const QString& tag, const QString& message) {
    QMutexLocker locker(&mutex_);
    QString logLine = QString("[%1] [%2] [%3] %4")
        .arg(getTimestamp(), levelToString(level), tag, message);

    // 输出到控制台
    switch (level) {
    case LogLevel::Debug:
        qDebug().noquote() << logLine;
        break;
    case LogLevel::Info:
        qInfo().noquote() << logLine;
        break;
    case LogLevel::Warning:
        qWarning().noquote() << logLine;
        break;
    case LogLevel::Error:
        std::cerr << logLine.toStdString() << std::endl;
        break;
    }

    // 写入文件
    writeToFile(logLine);
}

void Logger::debug(const QString& tag, const QString& message) {
    log(LogLevel::Debug, tag, message);
}

void Logger::info(const QString& tag, const QString& message) {
    log(LogLevel::Info, tag, message);
}

void Logger::warning(const QString& tag, const QString& message) {
    log(LogLevel::Warning, tag, message);
}

void Logger::error(const QString& tag, const QString& message) {
    log(LogLevel::Error, tag, message);
}

QString Logger::levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error:   return "ERROR";
    }
    return "UNKNOWN";
}

QString Logger::getTimestamp() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

void Logger::writeToFile(const QString& line) {
    if (logFile_ && logFile_->isOpen()) {
        QTextStream stream(logFile_);
        stream.setEncoding(QStringConverter::Utf8);
        stream << line << Qt::endl;
        logFile_->flush();
    }
}

} // namespace impress
