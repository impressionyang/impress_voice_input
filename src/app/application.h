#pragma once

#include <QApplication>
#include <memory>

namespace impress {

class ConfigManager;
class SenseVoiceEngine;

/**
 * @brief 应用入口封装
 *
 * 管理全局共享组件：配置管理器、STT 引擎。
 * STT 引擎在启动时异步加载模型，所有页面和服务共享同一实例。
 */
class Application : public QApplication {
    Q_OBJECT
public:
    Application(int& argc, char** argv);
    ~Application() override;

    /** @brief 获取全局配置管理器 */
    ConfigManager* configManager() const;

    /** @brief 获取全局 STT 引擎（共享实例） */
    SenseVoiceEngine* sttEngine() const;

    /** @brief 获取当前模型路径 */
    QString modelPath() const { return modelPath_; }

    /** @brief 获取全局 STT 引擎加载状态 */
    bool isModelLoaded() const;

signals:
    /** @brief 模型加载中（带路径） */
    void modelLoading(const QString& modelPath);

    /** @brief 模型加载完成（带路径） */
    void modelLoaded(const QString& modelPath);

    /** @brief 模型加载失败 */
    void modelLoadError(const QString& error);

private:
    void loadGlobalModel();

    std::unique_ptr<ConfigManager> configManager_;
    SenseVoiceEngine* sttEngine_ = nullptr;
    QString modelPath_;
    bool modelLoaded_ = false;
};

} // namespace impress
