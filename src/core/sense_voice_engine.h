#pragma once

#include <QObject>
#include <QString>
#include <vector>
#include <memory>
#include "stt_engine.h" // RecognitionResult 定义

namespace impress {

/**
 * @brief SenseVoice STT 推理引擎
 *
 * 封装 ONNX Runtime 推理逻辑，针对 SenseVoice 模型优化。
 * 完整的推理管线：PCM → Fbank → LFR → CMVN → ONNX → CTC 解码 → 文本。
 */
class SenseVoiceEngine : public QObject {
    Q_OBJECT
public:
    explicit SenseVoiceEngine(QObject* parent = nullptr);
    ~SenseVoiceEngine() override;

    /** @brief 同步加载模型 */
    bool loadModelSync(const QString& modelPath,
                       const QString& tokensPath = QString(),
                       const QString& device = "cpu",
                       int numThreads = 4);

    /** @brief 异步加载模型（后台线程，不阻塞 UI） */
    void loadModelAsync(const QString& modelPath,
                        const QString& tokensPath = QString(),
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
     * @param language 识别语言代码（"zh", "en", "ja", "ko", "yue", "auto"），空则自动
     */
    RecognitionResult infer(const std::vector<float>& samples,
                            int sampleRate,
                            const QString& language = QString());

    /** @brief 设置调试模式：开启后每次推理保存音频到 WAV */
    void setDebugSaveAudio(bool enable);

signals:
    void modelLoaded(const QString& modelPath);
    void modelLoadError(const QString& modelPath, const QString& error);
    void modelUnloaded();
    void error(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool loaded_ = false;
    bool debugSaveAudio_ = false;
};

} // namespace impress
