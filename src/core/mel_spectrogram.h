#pragma once

#include <vector>
#include <string>

namespace impress {

/**
 * @brief Mel 频谱图提取器
 *
 * 将音频 PCM 数据转换为 Whisper 模型所需的 Mel 频谱图。
 * 使用 Hann 窗口 + FFT + Mel 滤波器组。
 */
class MelSpectrogram {
public:
    /**
     * @brief 构造函数
     * @param nMel 滤波器数量，Whisper 使用 80
     * @param nFFT FFT 窗口大小
     * @param hopLength 帧移步长
     * @param sampleRate 采样率
     */
    MelSpectrogram(int nMel = 80, int nFFT = 400, int hopLength = 160, int sampleRate = 16000);

    /**
     * @brief 计算 Mel 频谱图
     * @param samples 归一化 PCM 浮点数据 [-1, 1]
     * @return Mel 频谱图数据，维度 [nMel x nFrames]
     */
    std::vector<float> compute(const std::vector<float>& samples) const;

    /** @brief 获取帧数 */
    int nFrames(int numSamples) const;

    /** @brief Mel 滤波器组数量 */
    int nMel() const { return nMel_; }

private:
    std::vector<float> hannWindow(int size) const;
    std::vector<float> melFilterbank() const;
    std::vector<float> stft(const std::vector<float>& samples, int frameStart) const;
    static float hzToMel(float hz);
    static float melToHz(float mel);

    int nMel_;
    int nFFT_;
    int hopLength_;
    int sampleRate_;
    int nFFTWindow_;      // 实际 FFT 大小（向上取 2 的幂）
    int preemphasisCoeff_ = 0;  // Whisper 不使用预加重
};

} // namespace impress
