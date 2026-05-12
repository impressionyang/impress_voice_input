#include "string_utils.h"
#include <sstream>
#include <iomanip>

namespace impress {

QStringList StringUtils::split(const QString& input, const QString& delimiter) {
    return input.split(delimiter, Qt::SkipEmptyParts);
}

QString StringUtils::trim(const QString& input) {
    return input.trimmed();
}

QString StringUtils::joinFloats(const std::vector<float>& values, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision);
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << values[i];
    }
    return QString::fromStdString(oss.str());
}

std::string StringUtils::toUtf8(const QString& input) {
    return input.toStdString();
}

QString StringUtils::fromUtf8(const std::string& input) {
    return QString::fromUtf8(input.c_str(), static_cast<int>(input.size()));
}

QString StringUtils::formatFileSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

QString StringUtils::formatDuration(int totalSeconds) {
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

} // namespace impress
