#include "sense_voice_engine.h"
#include "sense_voice_features.h"
#include "sense_voice_tokenizer.h"
#include "sense_voice_cmvn.h"
#include "audio_processor.h"
#include "utils/logger.h"
#include "utils/timer.h"

#include <QThread>
#include <QFuture>
#include <QtConcurrent>
#include <QMutex>
#include <QMutexLocker>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

// ONNX Runtime headers
#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

static const char* const kTag = "SenseVoiceEngine";

namespace impress {

/** 语言代码映射 */
static int languageToInt(const QString& lang) {
    if (lang.isEmpty()) return kLangAuto;
    if (lang == "zh") return kLangZh;
    if (lang == "en") return kLangEn;
    if (lang == "ja") return kLangJa;
    if (lang == "ko") return kLangKo;
    if (lang == "yue") return kLangYue;
    if (lang == "auto") return kLangAuto;
    return kLangAuto;
}

/**
 * @brief SenseVoice 引擎内部实现
 */
struct SenseVoiceEngine::Impl {
#ifdef HAVE_ONNXRUNTIME
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::SessionOptions> sessionOptions;
    std::unique_ptr<Ort::Session> session;

    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;

    SenseVoiceTokenizer tokenizer;
    std::unique_ptr<SenseVoiceFeatures> features;

    bool loadInWorker(const QString& modelPath,
                      const QString& tokensPath,
                      const QString& device,
                      int numThreads,
                      QString& errorMsg)
    {
        QMutexLocker locker(&mutex);
        try {
            auto envPtr = std::make_unique<Ort::Env>(
                ORT_LOGGING_LEVEL_WARNING, "impress_sensevoice");
            auto optionsPtr = std::make_unique<Ort::SessionOptions>();
            optionsPtr->SetIntraOpNumThreads(numThreads);
            optionsPtr->SetGraphOptimizationLevel(
                GraphOptimizationLevel::ORT_ENABLE_ALL);

            if (device == "gpu") {
                LOG_WARNING(kTag, "GPU 加速尚未实现，回退到 CPU");
            }

            LOG_INFO(kTag, QString("正在加载 SenseVoice 模型: %1 (线程: %2)")
                .arg(modelPath).arg(numThreads));

            auto sessionPtr = std::make_unique<Ort::Session>(
                *envPtr,
                modelPath.toUtf8().constData(),
                *optionsPtr);

            Ort::AllocatorWithDefaultOptions allocator;
            size_t inputCount = sessionPtr->GetInputCount();
            size_t outputCount = sessionPtr->GetOutputCount();

            LOG_INFO(kTag, QString("模型有 %1 个输入, %2 个输出")
                .arg(inputCount).arg(outputCount));

            inputNames.clear();
            outputNames.clear();

            for (size_t i = 0; i < inputCount; i++) {
                auto namePtr = sessionPtr->GetInputNameAllocated(i, allocator);
                inputNames.emplace_back(namePtr.get());
                LOG_DEBUG(kTag, QString("输入 #%1: %2").arg(i).arg(namePtr.get()));
            }

            for (size_t i = 0; i < outputCount; i++) {
                auto namePtr = sessionPtr->GetOutputNameAllocated(i, allocator);
                outputNames.emplace_back(namePtr.get());
                LOG_DEBUG(kTag, QString("输出 #%1: %2").arg(i).arg(namePtr.get()));
            }

            env = std::move(envPtr);
            sessionOptions = std::move(optionsPtr);
            session = std::move(sessionPtr);

            // 加载 tokenizer 词表
            QString vocabPath = tokensPath;
            if (vocabPath.isEmpty()) {
                QFileInfo modelInfo(modelPath);
                vocabPath = modelInfo.absolutePath() + "/tokens.txt";
            }
            if (QFile::exists(vocabPath)) {
                tokenizer.load(vocabPath);
                LOG_INFO(kTag, QString("Tokenizer 词表已加载: %1").arg(vocabPath));
            } else {
                LOG_WARNING(kTag, QString("未找到 tokenizer 词表: %1").arg(vocabPath));
            }

            // 初始化特征提取器
            features = std::make_unique<SenseVoiceFeatures>(16000);

            LOG_INFO(kTag, QString("SenseVoice 模型加载成功: %1").arg(modelPath));
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
    }

    QMutex mutex;
#endif
};

SenseVoiceEngine::SenseVoiceEngine(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{}

SenseVoiceEngine::~SenseVoiceEngine() {
    unloadModel();
}

bool SenseVoiceEngine::loadModelSync(const QString& modelPath,
                                      const QString& tokensPath,
                                      const QString& device,
                                      int numThreads)
{
    if (loaded_) {
        LOG_WARNING(kTag, "模型已加载，先卸载再加载");
        unloadModel();
    }

    QString errorMsg;
    bool success = impl_->loadInWorker(modelPath, tokensPath, device, numThreads, errorMsg);
    loaded_ = success;

    if (success) {
        emit modelLoaded(modelPath);
    } else {
        emit modelLoadError(modelPath, errorMsg);
        emit error(errorMsg);
    }
    return success;
}

void SenseVoiceEngine::loadModelAsync(const QString& modelPath,
                                       const QString& tokensPath,
                                       const QString& device,
                                       int numThreads)
{
    if (loaded_) {
        LOG_WARNING(kTag, "模型已加载，先卸载再加载");
        unloadModel();
    }

    LOG_INFO(kTag, QString("异步加载 SenseVoice 模型: %1").arg(modelPath));

    QFuture<void> future = QtConcurrent::run([this, modelPath, tokensPath, device, numThreads]() {
        QString errorMsg;
        bool success = impl_->loadInWorker(modelPath, tokensPath, device, numThreads, errorMsg);

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

void SenseVoiceEngine::unloadModel() {
    QMutexLocker locker(&impl_->mutex);
#ifdef HAVE_ONNXRUNTIME
    impl_->session.reset();
    impl_->sessionOptions.reset();
    impl_->env.reset();
    impl_->features.reset();
    impl_->tokenizer = SenseVoiceTokenizer();
#endif
    loaded_ = false;
    LOG_INFO(kTag, "模型已卸载");
    emit modelUnloaded();
}

bool SenseVoiceEngine::isLoaded() const {
    return loaded_;
}

/** CTC 贪婪解码：去重 + 去除空白 */
static std::vector<int> ctcGreedyDecode(const std::vector<int>& tokens, int blankToken) {
    std::vector<int> result;
    int prev = -1;

    for (int token : tokens) {
        if (token == blankToken) {
            prev = -1; // 重置去重状态
            continue;
        }
        if (token != prev) {
            result.push_back(token);
        }
        prev = token;
    }

    return result;
}

/** argmax: 寻找数组中最大值的索引 */
static int argmax(const float* data, int start, int end) {
    int bestIdx = start;
    float bestVal = data[start];
    for (int i = start + 1; i < end; i++) {
        if (data[i] > bestVal) {
            bestVal = data[i];
            bestIdx = i;
        }
    }
    return bestIdx;
}

RecognitionResult SenseVoiceEngine::infer(const std::vector<float>& samples,
                                           int sampleRate,
                                           const QString& language)
{
    Timer timer;
    RecognitionResult result;

    QString lang = language.isEmpty() ? "auto" : language;
    LOG_DEBUG(kTag, QString("推理语言: %1 (采样率: %2Hz, 样本数: %3)")
        .arg(lang).arg(sampleRate).arg(samples.size()));

#ifdef HAVE_ONNXRUNTIME
    if (!loaded_) {
        result.text = "[错误] 模型未加载";
        result.latency_ms = timer.elapsedMs();
        return result;
    }

    if (samples.empty()) {
        result.text = "";
        result.latency_ms = timer.elapsedMs();
        return result;
    }

    try {
        // 1. 重采样到 16kHz
        Timer preprocessTimer;
        std::vector<float> processedSamples = samples;
        int currentSampleRate = sampleRate;

        if (sampleRate != 16000) {
            AudioProcessor processor(16000);
            processedSamples = processor.resample(samples, sampleRate);
            currentSampleRate = 16000;
            LOG_DEBUG(kTag, QString("重采样: %1Hz -> %2Hz (%3 -> %4 样本)")
                .arg(sampleRate).arg(currentSampleRate)
                .arg(samples.size()).arg(processedSamples.size()));
        }

        // 2. 提取 LFR Fbank 特征
        std::vector<float> lfrFeatures = impl_->features->extract(processedSamples);
        int numFrames = static_cast<int>(lfrFeatures.size()) / kLFROutputDim;
        LOG_DEBUG(kTag, QString("特征提取: %1 ms (%2 帧, %3-dim)")
            .arg(preprocessTimer.elapsedMs(), 0, 'f', 1)
            .arg(numFrames).arg(kLFROutputDim));

        if (numFrames <= 0) {
            result.text = "[错误] 特征提取失败";
            result.latency_ms = timer.elapsedMs();
            return result;
        }

        // 3. 准备输入张量
        QMutexLocker locker(&impl_->mutex);

        // 输入: x, x_length, language, text_norm
        int64_t xShape[] = {1, numFrames, kLFROutputDim};
        auto memInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

        int64_t xLengthVal = numFrames;
        int64_t xLengthShape[] = {1};

        int langCode = languageToInt(lang);
        int64_t langVal = langCode;
        int64_t langShape[] = {1};

        int64_t textNormVal = kTextNormWithITN;
        int64_t textNormShape[] = {1};

        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(Ort::Value::CreateTensor<float>(
            memInfo, lfrFeatures.data(), lfrFeatures.size(), xShape, 3));
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memInfo, &xLengthVal, 1, xLengthShape, 1));
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memInfo, &langVal, 1, langShape, 1));
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memInfo, &textNormVal, 1, textNormShape, 1));

        // 4. 运行推理
        Timer inferTimer;
        std::vector<const char*> inputNamePtrs;
        for (auto& name : impl_->inputNames) inputNamePtrs.push_back(name.c_str());
        std::vector<const char*> outputNamePtrs;
        for (auto& name : impl_->outputNames) outputNamePtrs.push_back(name.c_str());

        auto outputTensors = impl_->session->Run(
            Ort::RunOptions{nullptr},
            inputNamePtrs.data(), inputTensors.data(), inputTensors.size(),
            outputNamePtrs.data(), outputNamePtrs.size());

        LOG_DEBUG(kTag, QString("ONNX 推理: %1 ms").arg(inferTimer.elapsedMs(), 0, 'f', 1));

        // 5. 解析输出 logits [1, seq_len, 25055]
        auto& outputTensor = outputTensors[0];
        auto shape = outputTensor.GetTensorTypeAndShapeInfo().GetShape();
        const float* logitsData = outputTensor.GetTensorData<float>();

        LOG_DEBUG(kTag, QString("输出维度: [%1, %2, %3]")
            .arg(shape[0]).arg(shape[1]).arg(shape[2]));

        int seqLen = static_cast<int>(shape[1]);
        int vocabSize = static_cast<int>(shape[2]);

        // 6. CTC 贪婪解码
        std::vector<int> rawTokens;
        float totalConf = 0.0f;
        int confCount = 0;

        for (int t = 0; t < seqLen; t++) {
            int offset = t * vocabSize;
            int bestToken = argmax(logitsData, offset, offset + vocabSize);

            if (bestToken != SenseVoiceTokenizer::kTokenBlank) {
                rawTokens.push_back(bestToken);

                // 计算置信度
                float maxLogit = logitsData[offset + bestToken];
                // 近似置信度: 使用 softmax 的最大值位置
                totalConf += maxLogit;
                confCount++;
            }
        }

        // CTC 去重
        std::vector<int> decodedTokens = ctcGreedyDecode(rawTokens, SenseVoiceTokenizer::kTokenBlank);

        // 计算平均置信度 (softmax)
        if (confCount > 0) {
            float avgLogit = totalConf / confCount;
            // 归一化到 0-1 范围
            result.confidence = 1.0f / (1.0f + std::exp(-avgLogit));
        }

        // 7. 解码 token 为文本
        if (decodedTokens.empty()) {
            result.text = "";
        } else if (impl_->tokenizer.isLoaded()) {
            result.text = impl_->tokenizer.decode(decodedTokens);
            LOG_DEBUG(kTag, QString("解码文本: %1 个 token → %2 字符")
                .arg(decodedTokens.size()).arg(result.text.length()));
        } else {
            // 降级：输出 token ID
            QString decodedText;
            for (int token : decodedTokens) {
                if (!decodedText.isEmpty()) decodedText += " ";
                decodedText += QString::number(token);
            }
            result.text = decodedText;
            LOG_WARNING(kTag, "Tokenizer 未加载，使用 token ID 输出");
        }

        result.isFinal = true;

    } catch (const std::exception& e) {
        result.text = QString("[错误] 推理失败: %1").arg(e.what());
        LOG_ERROR(kTag, result.text);
    }
#else
    result.text = "[占位] ONNX Runtime 未启用";
#endif

    result.latency_ms = timer.elapsedMs();
    LOG_DEBUG(kTag, QString("推理总耗时: %1 ms").arg(result.latency_ms, 0, 'f', 1));
    return result;
}

} // namespace impress
