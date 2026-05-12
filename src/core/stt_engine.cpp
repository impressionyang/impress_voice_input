#include "stt_engine.h"
#include "utils/logger.h"
#include "utils/timer.h"

#include <QThread>
#include <QFuture>
#include <QtConcurrent>
#include <QMutex>
#include <QMutexLocker>

// ONNX Runtime headers
#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

static const char* const kTag = "STTEngine";

namespace impress {

struct STTEngine::Impl {
#ifdef HAVE_ONNXRUNTIME
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::SessionOptions> sessionOptions;
    std::unique_ptr<Ort::Session> session;
#endif
    QMutex mutex;

    /**
     * @brief 在后台线程中执行模型加载
     * 返回 true 表示成功，false 表示失败
     */
    bool loadInWorker(const QString& modelPath,
                      const QString& device,
                      int numThreads,
                      QString& errorMsg)
    {
#ifdef HAVE_ONNXRUNTIME
        QMutexLocker locker(&mutex);
        try {
            auto envPtr = std::make_unique<Ort::Env>(
                ORT_LOGGING_LEVEL_WARNING, "impress_voice");
            auto optionsPtr = std::make_unique<Ort::SessionOptions>();
            optionsPtr->SetIntraOpNumThreads(numThreads);
            optionsPtr->SetGraphOptimizationLevel(
                GraphOptimizationLevel::ORT_ENABLE_ALL);

            if (device == "gpu") {
                LOG_WARNING(kTag, "GPU 加速尚未实现，回退到 CPU");
            }

            LOG_INFO(kTag, QString("正在加载模型: %1 (线程: %2)").arg(modelPath).arg(numThreads));

            // ONNX Session 构造函数在 Linux 上使用 const char* 路径
            auto sessionPtr = std::make_unique<Ort::Session>(
                *envPtr,
                modelPath.toUtf8().constData(),
                *optionsPtr);

            // 全部成功后才替换成员变量
            env = std::move(envPtr);
            sessionOptions = std::move(optionsPtr);
            session = std::move(sessionPtr);

            LOG_INFO(kTag, QString("模型加载成功: %1").arg(modelPath));
            return true;
        } catch (const Ort::Exception& e) {
            errorMsg = QString("ONNX 异常: %1").arg(e.what());
            LOG_ERROR(kTag, errorMsg);
            return false;
        } catch (const std::exception& e) {
            errorMsg = QString("加载异常: %1").arg(e.what());
            LOG_ERROR(kTag, errorMsg);
            return false;
        }
#else
        errorMsg = "ONNX Runtime 未编译启用";
        LOG_ERROR(kTag, errorMsg);
        return false;
#endif
    }
};

STTEngine::STTEngine(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{}

STTEngine::~STTEngine() {
    unloadModel();
}

bool STTEngine::loadModelSync(const QString& modelPath,
                              const QString& device,
                              int numThreads)
{
    if (loaded_) {
        LOG_WARNING(kTag, "模型已加载，先卸载再加载新模型");
        unloadModel();
    }

    QString errorMsg;
    bool success = impl_->loadInWorker(modelPath, device, numThreads, errorMsg);
    loaded_ = success;

    if (success) {
        emit modelLoaded(modelPath);
    } else {
        emit modelLoadError(modelPath, errorMsg);
        emit error(errorMsg);
    }
    return success;
}

void STTEngine::loadModelAsync(const QString& modelPath,
                               const QString& device,
                               int numThreads)
{
    if (loaded_) {
        LOG_WARNING(kTag, "模型已加载，先卸载再加载新模型");
        unloadModel();
    }

    LOG_INFO(kTag, QString("异步加载模型: %1").arg(modelPath));

    // 在后台线程中执行加载
    QFuture<void> future = QtConcurrent::run([this, modelPath, device, numThreads]() {
        QString errorMsg;
        bool success = impl_->loadInWorker(modelPath, device, numThreads, errorMsg);

        // 回到主线程发送信号
        QMetaObject::invokeMethod(this, [this, modelPath, errorMsg, success]() {
            loaded_ = success;
            if (success) {
                emit modelLoaded(modelPath);
            } else {
                emit modelLoadError(modelPath, errorMsg);
                emit error(errorMsg);
            }
        }, Qt::QueuedConnection);
    });
}

void STTEngine::unloadModel() {
    QMutexLocker locker(&impl_->mutex);
#ifdef HAVE_ONNXRUNTIME
    impl_->session.reset();
    impl_->sessionOptions.reset();
    impl_->env.reset();
#endif
    loaded_ = false;
    LOG_INFO(kTag, "模型已卸载");
    emit modelUnloaded();
}

bool STTEngine::isLoaded() const {
    return loaded_;
}

RecognitionResult STTEngine::infer(const std::vector<float>& samples,
                                   int sampleRate,
                                   bool isStreaming)
{
    Timer timer;
    RecognitionResult result;

#ifdef HAVE_ONNXRUNTIME
    if (!loaded_) {
        result.text = "[错误] 模型未加载";
        result.latency_ms = timer.elapsedMs();
        return result;
    }

    try {
        // 标记未使用的参数，消除编译警告
        (void)samples;
        (void)sampleRate;
        (void)isStreaming;

        // TODO: 实现完整的 ONNX 推理流程
        // 1. 创建输入 Tensor
        // 2. 运行推理
        // 3. 解码输出 (CTC / 自回归)
        // 4. Tokenizer 解码文本

        result.text = "[占位] 推理逻辑待实现";
        result.confidence = 0.95f;
        result.isFinal = true;
    } catch (const std::exception& e) {
        result.text = QString("[错误] 推理失败: %1").arg(e.what());
    }
#else
    result.text = "[占位] ONNX Runtime 未启用，推理逻辑未实现";
#endif

    result.latency_ms = timer.elapsedMs();
    LOG_DEBUG(kTag, QString("推理耗时: %1 ms").arg(result.latency_ms, 0, 'f', 1));
    return result;
}

} // namespace impress
