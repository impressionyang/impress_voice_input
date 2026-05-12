#include "application.h"
#include "config_manager.h"
#include "utils/logger.h"
#include <QFile>

static const char* const kTag = "Application";

namespace impress {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    LOG_INFO(kTag, "Impress Voice Input 启动");

    configManager_ = std::make_unique<ConfigManager>(this);
    configManager_->loadDefaults();
}

Application::~Application() {
    LOG_INFO(kTag, "应用退出");
    Logger::shutdown();
}

ConfigManager* Application::configManager() const {
    return configManager_.get();
}

} // namespace impress
