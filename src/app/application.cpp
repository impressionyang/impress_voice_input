#include "application.h"
#include "config_manager.h"
#include "core/sense_voice_engine.h"
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

    // 创建全局 STT 引擎（共享实例）
    sttEngine_ = new SenseVoiceEngine(this);
    connect(sttEngine_, &SenseVoiceEngine::modelLoaded, this, [this](const QString& path) {
        modelLoaded_ = true;
        modelPath_ = path;
        LOG_INFO(kTag, QString("全局模型已加载: %1").arg(path));
        emit modelLoaded(path);
    });
    connect(sttEngine_, &SenseVoiceEngine::modelLoadError, this, [this](const QString&, const QString& err) {
        modelLoaded_ = false;
        LOG_ERROR(kTag, QString("全局模型加载失败: %1").arg(err));
        emit modelLoadError(err);
    });

    // 异步加载全局模型
    loadGlobalModel();
}

Application::~Application() {
    LOG_INFO(kTag, "应用退出");
    Logger::shutdown();
}

ConfigManager* Application::configManager() const {
    return configManager_.get();
}

SenseVoiceEngine* Application::sttEngine() const {
    return sttEngine_;
}

bool Application::isModelLoaded() const {
    return modelLoaded_;
}

void Application::loadGlobalModel() {
    QString modelPath = configManager_->get("stt.model_path").toString();
    if (modelPath.isEmpty()) {
        LOG_WARNING(kTag, "模型路径为空，请在配置中设置后重启");
        emit modelLoadError("模型路径未设置");
        return;
    }

    modelPath_ = modelPath;

    QString tokensPath = configManager_->get("stt.tokens_path").toString();
    QString device = configManager_->get("stt.device").toString();
    int numThreads = configManager_->get("stt.num_threads").toInt();

    bool debugSave = configManager_->get("stt.debug_save_audio").toBool();
    sttEngine_->setDebugSaveAudio(debugSave);

    LOG_INFO(kTag, QString("正在异步加载全局模型: %1").arg(modelPath));
    emit modelLoading(modelPath);
    sttEngine_->loadModelAsync(modelPath, tokensPath, device, numThreads);
}

} // namespace impress
