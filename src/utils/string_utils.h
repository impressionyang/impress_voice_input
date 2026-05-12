#pragma once

#include <QString>
#include <QStringList>
#include <vector>

namespace impress {

/**
 * @brief 字符串工具函数
 */
class StringUtils {
public:
    /** @brief 按分隔符分割字符串 */
    static QStringList split(const QString& input, const QString& delimiter);

    /** @brief 去除两端空白 */
    static QString trim(const QString& input);

    /** @brief float 向量转 QString（逗号分隔） */
    static QString joinFloats(const std::vector<float>& values, int precision = 4);

    /** @brief QString 转 UTF-8 std::string */
    static std::string toUtf8(const QString& input);

    /** @brief std::string 转 QString */
    static QString fromUtf8(const std::string& input);

    /** @brief 格式化文件大小 */
    static QString formatFileSize(qint64 bytes);

    /** @brief 格式化时长 (mm:ss) */
    static QString formatDuration(int totalSeconds);
};

} // namespace impress
