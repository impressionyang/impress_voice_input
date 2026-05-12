#pragma once

#include <vector>

namespace impress {

/**
 * @brief 音频预处理模块
 *
 * 负责音频重采样、归一化、分帧等操作。
 */
class AudioProcessor {
public:
    AudioProcessor(int targetSampleRate = 16000);

    /** @brief 重采样到目标采样率 */
    std::vector<float> resample(const std::vector<float>& input,
                                int sourceSampleRate);

    /** @brief 将 PCM 数据归一化到 [-1, 1] */
    static std::vector<float> normalize(const std::vector<short>& pcm16);

    /** @brief 将 PCM 浮点数据归一化到 [-1, 1] */
    static std::vector<float> normalizeFloats(const std::vector<float>& input);

    /** @brief 将音频切分为重叠帧 */
    std::vector<std::vector<float>> frame(const std::vector<float>& input,
                                          int frameSize,
                                          int hopSize);

    /** @brief 获取目标采样率 */
    int targetSampleRate() const { return targetSampleRate_; }

private:
    int targetSampleRate_;
};

} // namespace impress
