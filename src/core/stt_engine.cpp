#include "stt_engine.h"
#include "mel_spectrogram.h"
#include "whisper_tokenizer.h"
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
#include <cstring>

// ONNX Runtime headers
#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

static const char* const kTag = "STTEngine";

// Whisper 常量
static const int kMaxTokens = 224;
static const int kMelBins = 80;

namespace impress {

/**
 * @brief STT 引擎内部实现
 */
struct STTEngine::Impl {
#ifdef HAVE_ONNXRUNTIME
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::SessionOptions> sessionOptions;
    std::unique_ptr<Ort::Session> session;

    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;

    WhisperTokenizer tokenizer;
    QString currentLanguage;

    bool loadInWorker(const QString& modelPath,
                      const QString& device,
                      int numThreads,
                      QString& errorMsg)
    {
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

            // 尝试加载同目录下的 tokenizer 词表
            QFileInfo modelInfo(modelPath);
            QString vocabPath = modelInfo.absolutePath() + "/tokenizer.vocab";
            if (QFile::exists(vocabPath)) {
                tokenizer.loadVocabulary(vocabPath);
                LOG_INFO(kTag, "Tokenizer 词表已加载");
            } else {
                LOG_WARNING(kTag, QString("未找到 tokenizer 词表: %1").arg(vocabPath));
            }

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
    }

    QMutex mutex;
#endif
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

    QFuture<void> future = QtConcurrent::run([this, modelPath, device, numThreads]() {
        QString errorMsg;
        bool success = impl_->loadInWorker(modelPath, device, numThreads, errorMsg);

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
    impl_->tokenizer = WhisperTokenizer();
    impl_->currentLanguage.clear();
#endif
    loaded_ = false;
    LOG_INFO(kTag, "模型已卸载");
    emit modelUnloaded();
}

bool STTEngine::isLoaded() const {
    return loaded_;
}

int STTEngine::vocabSize() const {
    return 51865;
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

/** softmax 计算 */
static std::vector<float> softmax(const float* data, int start, int end) {
    float maxVal = -1e9f;
    for (int i = start; i < end; i++) {
        maxVal = std::max(maxVal, data[i]);
    }
    float sum = 0.0f;
    std::vector<float> probs(end - start);
    for (int i = start; i < end; i++) {
        probs[i - start] = std::exp(data[i] - maxVal);
        sum += probs[i - start];
    }
    for (float& p : probs) p /= sum;
    return probs;
}

RecognitionResult STTEngine::infer(const std::vector<float>& samples,
                                   int sampleRate,
                                   const QString& language)
{
    Timer timer;
    RecognitionResult result;

    QString lang = language.isEmpty() ? "zh" : language;
    LOG_DEBUG(kTag, QString("推理语言: %1 (采样率: %2Hz, 样本数: %3)")
        .arg(lang).arg(sampleRate).arg(samples.size()));

#ifdef HAVE_ONNXRUNTIME
    if (!loaded_) {
        result.text = "[错误] 模型未加载";
        result.latency_ms = timer.elapsedMs();
        return result;
    }

    try {
        // 1. 重采样到 Whisper 所需的 16kHz
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

        // 2. Mel 频谱图提取
        MelSpectrogram melExtractor(kMelBins, 400, 160, currentSampleRate);
        std::vector<float> melSpec = melExtractor.compute(processedSamples);
        int nFrames = melExtractor.nFrames(static_cast<int>(processedSamples.size()));
        if (nFrames <= 0) nFrames = 1;
        LOG_DEBUG(kTag, QString("Mel 计算: %1 ms (%2 帧)")
            .arg(preprocessTimer.elapsedMs(), 0, 'f', 1).arg(nFrames));

        // 3. 运行 ONNX 推理
        Timer inferTimer;
        QMutexLocker locker(&impl_->mutex);

        int64_t melShape[] = {1, kMelBins, static_cast<int64_t>(nFrames)};
        auto memInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(Ort::Value::CreateTensor<float>(
            memInfo, melSpec.data(), melSpec.size(), melShape, 3));

        std::vector<const char*> inputNamePtrs;
        for (auto& name : impl_->inputNames) inputNamePtrs.push_back(name.c_str());
        std::vector<const char*> outputNamePtrs;
        for (auto& name : impl_->outputNames) outputNamePtrs.push_back(name.c_str());

        auto outputTensors = impl_->session->Run(
            Ort::RunOptions{nullptr},
            inputNamePtrs.data(), inputTensors.data(), inputTensors.size(),
            outputNamePtrs.data(), impl_->outputNames.size());

        LOG_DEBUG(kTag, QString("ONNX 推理: %1 ms").arg(inferTimer.elapsedMs(), 0, 'f', 1));

        // 4. 解析输出
        auto& outputTensor = outputTensors[0];
        auto shape = outputTensor.GetTensorTypeAndShapeInfo().GetShape();
        const float* outputData = outputTensor.GetTensorMutableData<float>();

        LOG_DEBUG(kTag, QString("输出维度: %1").arg(shape.size()));
        for (size_t i = 0; i < shape.size(); i++) {
            LOG_DEBUG(kTag, QString("  dim[%1] = %2").arg(i).arg(shape[i]));
        }

        int vocabSize = 51865;
        std::vector<int> tokens;

        if (shape.size() == 2 && shape[1] == vocabSize) {
            // [1, vocab_size] - 直接输出
            int searchEnd = std::min(vocabSize, 50256);
            int bestToken = argmax(outputData, 0, searchEnd);
            if (!WhisperTokenizer::isSpecialToken(bestToken)) {
                tokens.push_back(bestToken);
            }
            auto probs = softmax(outputData, 0, searchEnd);
            float maxProb = probs[0];
            for (size_t i = 1; i < probs.size(); i++) {
                if (probs[i] > maxProb) maxProb = probs[i];
            }
            result.confidence = maxProb;

        } else if (shape.size() >= 3) {
            // [1, seq_len, vocab_size] - 自回归输出
            int seqLen = static_cast<int>(shape[1]);
            vocabSize = static_cast<int>(shape[2]);

            for (int t = 0; t < seqLen && static_cast<int>(tokens.size()) < kMaxTokens; t++) {
                int offset = t * vocabSize;
                int searchEnd = std::min(offset + vocabSize, offset + 50256);
                int bestToken = argmax(outputData, offset, searchEnd);
                if (WhisperTokenizer::isSpecialToken(bestToken)) break;
                if (!tokens.empty() && tokens.back() == bestToken) continue;
                tokens.push_back(bestToken);
            }

            if (!tokens.empty()) {
                float avgConf = 0.0f;
                for (int t = 0; t < seqLen && t < static_cast<int>(tokens.size()); t++) {
                    int offset = t * vocabSize;
                    auto probs = softmax(outputData, offset, offset + vocabSize);
                    int bestToken = argmax(outputData, offset, offset + vocabSize);
                    avgConf += probs[bestToken - offset];
                }
                result.confidence = avgConf / tokens.size();
            }
        } else {
            result.text = QString("[错误] 不支持的输出维度: %1").arg(shape.size());
            result.latency_ms = timer.elapsedMs();
            return result;
        }

        // 5. 解码 token 为文本
        if (tokens.empty()) {
            result.text = "";
        } else if (impl_->tokenizer.isLoaded()) {
            // 使用 tokenizer 解码
            result.text = impl_->tokenizer.decode(tokens);
        } else {
            // 降级：直接输出 token ID（用于调试）
            QString decodedText;
            for (int token : tokens) {
                if (token < 0 || token >= 50256) continue;
                decodedText += QString("[T%1]").arg(token);
            }
            result.text = decodedText;
            LOG_DEBUG(kTag, "Tokenizer 未加载，使用 token ID 输出");
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
