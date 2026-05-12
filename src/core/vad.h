#pragma once

#include <vector>

namespace impress {

/**
 * @brief 语音活动检测 (Voice Activity Detection)
 *
 * 基于短时能量和过零率的简单 VAD 实现。
 * 用于实时语音流中检测有效语音段，过滤静音。
 */
class VoiceActivityDetector {
public:
    /**
     * @brief 构造函数
     * @param sampleRate 采样率
     * @param frameMs 帧长度（毫秒）
     * @param energyThreshold 能量阈值（归一化，建议 0.01-0.05）
     * @param minVoiceFrames 判定为语音所需的最小连续帧数
     */
    VoiceActivityDetector(int sampleRate = 16000,
                          int frameMs = 30,
                          float energyThreshold = 0.02f,
                          int minVoiceFrames = 3);

    /**
     * @brief 处理一帧音频数据
     * @param samples 归一化 PCM 浮点数据 [-1, 1]
     * @return true = 检测到语音, false = 静音
     */
    bool process(const std::vector<float>& samples);

    /** @brief 获取当前帧能量 */
    float currentEnergy() const { return currentEnergy_; }

    /** @brief 获取过零率 */
    float zeroCrossingRate() const { return zeroCrossingRate_; }

    /** @brief 是否正在说话 */
    bool isSpeaking() const { return isSpeaking_; }

    /** @brief 连续语音帧计数 */
    int consecutiveVoiceFrames() const { return consecutiveVoiceFrames_; }

    /**
     * @brief 处理多帧音频数据（整段）
     * @param samples 归一化 PCM 浮点数据 [-1, 1]
     * @return 检测到的语音段起始/结束帧索引
     */
    struct SpeechSegment {
        int startFrame;
        int endFrame;
    };
    std::vector<SpeechSegment> processBatch(const std::vector<float>& samples);

private:
    float computeEnergy(const std::vector<float>& samples) const;
    float computeZeroCrossingRate(const std::vector<float>& samples) const;

    int sampleRate_;
    int frameSize_;
    float energyThreshold_;
    int minVoiceFrames_;

    float currentEnergy_ = 0.0f;
    float zeroCrossingRate_ = 0.0f;
    bool isSpeaking_ = false;
    int consecutiveVoiceFrames_ = 0;
};

} // namespace impress
