#pragma once

#include <QApplication>
#include <memory>

namespace impress {

class ConfigManager;

/**
 * @brief 应用入口封装
 */
class Application : public QApplication {
    Q_OBJECT
public:
    Application(int& argc, char** argv);
    ~Application() override;

    /** @brief 获取全局配置管理器 */
    ConfigManager* configManager() const;

private:
    std::unique_ptr<ConfigManager> configManager_;
};

} // namespace impress
