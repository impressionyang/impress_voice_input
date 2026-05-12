#pragma once

#include <QObject>
#include <QString>
#include <vector>
#include <memory>

namespace impress {

struct RecognitionResult {
    QString text;
    float confidence = 0.0f;
    double latency_ms = 0.0;
    bool isFinal = false;
};

/**
 * @brief STT 推理引擎
 *
 * 封装 ONNX Runtime 推理逻辑，负责模型加载、音频推理和结果输出。
 * 模型加载在后台线程执行，不阻塞 UI。
 */
class STTEngine : public QObject {
    Q_OBJECT
public:
    explicit STTEngine(QObject* parent = nullptr);
    ~STTEngine() override;

    /** @brief 同步加载模型（阻塞，不推荐在 UI 线程调用） */
    bool loadModelSync(const QString& modelPath,
                       const QString& device = "cpu",
                       int numThreads = 4);

    /** @brief 异步加载模型（后台线程，不阻塞 UI） */
    void loadModelAsync(const QString& modelPath,
                        const QString& device = "cpu",
                        int numThreads = 4);

    /** @brief 释放模型 */
    void unloadModel();

    /** @brief 是否已加载模型 */
    bool isLoaded() const;

    /**
     * @brief 推理音频数据
     * @param samples 归一化后的 PCM 浮点样本（范围 [-1, 1]）
     * @param sampleRate 采样率
     * @param isStreaming 是否流式推理
     */
    RecognitionResult infer(const std::vector<float>& samples,
                            int sampleRate,
                            bool isStreaming = true);

signals:
    void modelLoaded(const QString& modelPath);
    void modelLoadError(const QString& modelPath, const QString& error);
    void modelUnloaded();
    void error(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool loaded_ = false;
};

} // namespace impress
