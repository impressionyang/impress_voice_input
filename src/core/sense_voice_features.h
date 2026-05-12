#pragma once

#include <vector>

namespace impress {

/**
 * @brief SenseVoice 音频特征提取器
 *
 * 将原始 PCM 音频转换为 SenseVoice 模型所需的 LFR Fbank 特征。
 * 流程: PCM → 预加重 → 分帧 → FFT → Mel 滤波器 → 对数压缩 →
 *      LFR 拼接 → CMVN 归一化 → 560-dim 特征向量。
 */
class SenseVoiceFeatures {
public:
    /**
     * @brief 构造函数
     * @param sampleRate 输入音频采样率（默认 16000）
     */
    explicit SenseVoiceFeatures(int sampleRate = 16000);

    /**
     * @brief 从 PCM 数据提取 LFR Fbank 特征
     * @param samples 归一化 PCM 浮点数据 [-1, 1]
     * @return LFR Fbank 特征，维度 [nFrames * 560]
     */
    std::vector<float> extract(const std::vector<float>& samples) const;

    /** @brief 获取特征帧数 */
    int nFrames(int numSamples) const;

private:
    // Fbank 参数
    int sampleRate_;
    int nFft_ = 512;
    int nMel_ = 80;
    int hopLength_ = 160;      // 10ms @ 16kHz
    int winLength_ = 400;      // 25ms @ 16kHz
    float preEmphasisCoeff_ = 0.97f;

    // Mel 滤波器组 (预计算)
    struct MelFilter {
        int startBin;
        int endBin;
        std::vector<float> weights;
    };
    std::vector<MelFilter> melFilters_;

    std::vector<float> hannWindow() const;
    void buildMelFilters();
    void cmvn(std::vector<float>& features) const;
};

} // namespace impress
