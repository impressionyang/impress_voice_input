#pragma once

#include <QObject>
#include <QVariantMap>
#include <QMutex>

namespace impress {

/**
 * @brief 配置管理器
 *
 * 负责加载、保存、查询应用配置。配置以 JSON 格式存储。
 * 线程安全，支持跨线程读取。
 * 自动追踪配置文件路径，save() 无参调用即可持久化。
 */
class ConfigManager : public QObject {
    Q_OBJECT
public:
    explicit ConfigManager(QObject* parent = nullptr);

    /** @brief 从文件加载配置，并记录路径用于后续 save() */
    bool load(const QString& path);

    /** @brief 保存配置到上次加载/保存的路径 */
    bool save();

    /** @brief 保存配置到指定文件 */
    bool saveAs(const QString& path);

    /** @brief 使用默认配置初始化 */
    void loadDefaults();

    /** @brief 获取配置值（支持点号路径，如 "stt.model_path"） */
    QVariant get(const QString& key, const QVariant& defaultValue = {}) const;

    /** @brief 设置配置值 */
    void set(const QString& key, const QVariant& value);

    /** @brief 批量设置多个配置值（只发射一次 configChanged） */
    void setBatch(const QMap<QString, QVariant>& pairs);

    /** @brief 重置为默认配置 */
    void resetToDefaults();

    /** @brief 当前配置文件路径 */
    QString configPath() const { return configPath_; }

signals:
    void configChanged();

private:
    /** @brief 递归设置嵌套值 */
    static void setValue(QVariantMap& map, const QStringList& parts, int index, const QVariant& value);

    /** @brief 递归获取嵌套值 */
    static QVariant getValue(const QVariantMap& map, const QStringList& parts, int index, const QVariant& defaultValue);

    QString configPath_;
    mutable QMutex mutex_;
    QVariantMap config_;
};

} // namespace impress
