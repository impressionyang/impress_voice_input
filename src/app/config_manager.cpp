#include "config_manager.h"
#include "utils/logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>

static const char* const kTag = "ConfigManager";

namespace impress {

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{}

bool ConfigManager::load(const QString& path) {
    QMutexLocker locker(&mutex_);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(kTag, QString("无法打开配置文件: %1").arg(path));
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        LOG_ERROR(kTag, QString("JSON 解析错误: %1").arg(error.errorString()));
        return false;
    }

    config_ = doc.object().toVariantMap();
    configPath_ = path;
    LOG_INFO(kTag, QString("配置已加载: %1").arg(path));
    return true;
}

bool ConfigManager::save() {
    if (configPath_.isEmpty()) {
        // 自动生成默认路径
        QString appDataDir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        QDir().mkpath(appDataDir);
        configPath_ = appDataDir + "/config.json";
    }
    return saveAs(configPath_);
}

bool ConfigManager::saveAs(const QString& path) {
    QMutexLocker locker(&mutex_);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(kTag, QString("无法写入配置文件: %1").arg(path));
        return false;
    }

    QJsonDocument doc(QJsonObject::fromVariantMap(config_));
    file.write(doc.toJson(QJsonDocument::Indented));
    configPath_ = path;
    LOG_INFO(kTag, QString("配置已保存: %1").arg(path));
    return true;
}

void ConfigManager::loadDefaults() {
    {
        QMutexLocker locker(&mutex_);
        config_ = QVariantMap{
            {"stt", QVariantMap{
                {"model_path", ""},
                {"model_type", "sense_voice"},
                {"tokens_path", ""},
                {"device", "cpu"},
                {"num_threads", 4},
                {"sample_rate", 16000},
                {"language", "zh"},
                {"streaming", true},
                {"beam_size", 5},
                {"temperature", 0.0},
                {"debug_save_audio", false},
                {"capslock_voice_enabled", false}
            }},
            {"audio", QVariantMap{
                {"input_device", -1},
                {"buffer_size_ms", 20},
                {"chunk_duration_ms", 3000},
                {"padding_ms", 500}
            }},
            {"ui", QVariantMap{
                {"theme", "light"},
                {"font_size", 14},
                {"show_waveform", true},
                {"show_confidence", true}
            }},
            {"shortcuts", QVariantMap{
                {"voice_hotkey", "CapsLock"}
            }}
        };
    }
    emit configChanged();
}

QVariant ConfigManager::get(const QString& key, const QVariant& defaultValue) const {
    QMutexLocker locker(&mutex_);
    return getValue(config_, key.split('.'), 0, defaultValue);
}

QVariant ConfigManager::getValue(const QVariantMap& map, const QStringList& parts,
                                 int index, const QVariant& defaultValue)
{
    if (index >= parts.size() || !map.contains(parts[index])) {
        return defaultValue;
    }
    if (index == parts.size() - 1) {
        return map[parts[index]];
    }
    auto childMap = map[parts[index]].toMap();
    return getValue(childMap, parts, index + 1, defaultValue);
}

void ConfigManager::set(const QString& key, const QVariant& value) {
    {
        QMutexLocker locker(&mutex_);
        setValue(config_, key.split('.'), 0, value);
    }
    // 锁释放后再发射信号，防止槽函数中调用 get() 时死锁
    emit configChanged();
}

void ConfigManager::setBatch(const QMap<QString, QVariant>& pairs) {
    {
        QMutexLocker locker(&mutex_);
        for (auto it = pairs.constBegin(); it != pairs.constEnd(); ++it) {
            setValue(config_, it.key().split('.'), 0, it.value());
        }
    }
    // 锁释放后再发射信号，只发射一次
    emit configChanged();
}

void ConfigManager::setValue(QVariantMap& map, const QStringList& parts,
                             int index, const QVariant& value)
{
    if (index >= parts.size()) return;

    if (index == parts.size() - 1) {
        map.insert(parts[index], value);
        return;
    }

    if (!map.contains(parts[index]) || !map[parts[index]].canConvert<QVariantMap>()) {
        map.insert(parts[index], QVariantMap());
    }

    auto childMap = map[parts[index]].toMap();
    setValue(childMap, parts, index + 1, value);
    map[parts[index]] = childMap;
}

void ConfigManager::resetToDefaults() {
    loadDefaults();
    LOG_INFO(kTag, "配置已重置为默认值");
}

} // namespace impress
