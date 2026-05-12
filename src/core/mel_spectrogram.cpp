#include "mel_spectrogram.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Whisper 模型参数
static const int kWhisperDurationSec = 30;   // 30 秒音频
static const float kMinLevel = -11.5f;       // 对数谱图最小值

namespace impress {

// 简易复数运算
struct Complex {
    float re, im;
    Complex(float r = 0, float i = 0) : re(r), im(i) {}
    Complex operator+(const Complex& o) const { return {re + o.re, im + o.im}; }
    Complex operator-(const Complex& o) const { return {re - o.re, im - o.im}; }
    Complex operator*(const Complex& o) const {
        return {re * o.re - im * o.im, re * o.im + im * o.re};
    }
    Complex operator*(float s) const { return {re * s, im * s}; }
    float magnitudeSq() const { return re * re + im * im; }
};

/**
 * @brief Radix-2 Cooley-Tukey FFT
 */
static void fft(std::vector<Complex>& x) {
    int n = static_cast<int>(x.size());
    if (n <= 1) return;

    // 位反转置换
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }

    // 蝶形运算
    for (int len = 2; len <= n; len *= 2) {
        float angle = -2.0f * static_cast<float>(M_PI) / len;
        Complex wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            Complex w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; j++) {
                Complex u = x[i + j];
                Complex v = x[i + j + len / 2] * w;
                x[i + j] = u + v;
                x[i + j + len / 2] = u - v;
                w = w * wlen;
            }
        }
    }
}

MelSpectrogram::MelSpectrogram(int nMel, int nFFT, int hopLength, int sampleRate)
    : nMel_(nMel)
    , nFFT_(nFFT)
    , hopLength_(hopLength)
    , sampleRate_(sampleRate)
{
    // FFT 窗口大小向上取 2 的幂
    nFFTWindow_ = 1;
    while (nFFTWindow_ < nFFT) nFFTWindow_ *= 2;
}

int MelSpectrogram::nFrames(int numSamples) const {
    return (numSamples - nFFT_ + hopLength_) / hopLength_;
}

float MelSpectrogram::hzToMel(float hz) {
    return 1125.0f * std::log(1.0f + hz / 700.0f);
}

float MelSpectrogram::melToHz(float mel) {
    return 700.0f * (std::exp(mel / 1125.0f) - 1.0f);
}

std::vector<float> MelSpectrogram::hannWindow(int size) const {
    std::vector<float> window(size);
    for (int i = 0; i < size; i++) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (size - 1)));
    }
    return window;
}

std::vector<float> MelSpectrogram::melFilterbank() const {
    // Mel 滤波器组 [nMel x (nFFT/2 + 1)]
    int nFreq = nFFTWindow_ / 2 + 1;
    std::vector<float> filters(nMel_ * nFreq, 0.0f);

    float fMin = 0.0f;
    float fMax = static_cast<float>(sampleRate_) / 2.0f;
    float melMin = hzToMel(fMin);
    float melMax = hzToMel(fMax);

    // Mel 中心频率点
    std::vector<float> melPoints(nMel_ + 2);
    for (int i = 0; i < nMel_ + 2; i++) {
        float mel = melMin + (melMax - melMin) * i / (nMel_ + 1);
        melPoints[i] = melToHz(mel);
    }

    // 转换为 FFT bin 索引
    std::vector<int> binPoints(nMel_ + 2);
    for (int i = 0; i < nMel_ + 2; i++) {
        binPoints[i] = static_cast<int>(std::round((nFFTWindow_ + 1) * melPoints[i] / sampleRate_));
        binPoints[i] = std::min(binPoints[i], nFreq - 1);
    }

    // 构造三角滤波器
    for (int m = 0; m < nMel_; m++) {
        for (int k = 0; k < nFreq; k++) {
            float val = 0.0f;
            if (k >= binPoints[m] && k <= binPoints[m + 1]) {
                val = (k - binPoints[m]) / static_cast<float>(binPoints[m + 1] - binPoints[m] + 1e-10f);
            } else if (k >= binPoints[m + 1] && k <= binPoints[m + 2]) {
                val = (binPoints[m + 2] - k) / static_cast<float>(binPoints[m + 2] - binPoints[m + 1] + 1e-10f);
            }
            filters[m * nFreq + k] = val;
        }
    }

    // 归一化
    for (int m = 0; m < nMel_; m++) {
        float norm = 0.0f;
        for (int k = 0; k < nFreq; k++) {
            norm += filters[m * nFreq + k];
        }
        if (norm > 1e-10f) {
            for (int k = 0; k < nFreq; k++) {
                filters[m * nFreq + k] /= norm;
            }
        }
    }

    return filters;
}

std::vector<float> MelSpectrogram::stft(const std::vector<float>& samples, int frameStart) const {
    int nFreq = nFFTWindow_ / 2 + 1;
    std::vector<float> magnitude(nFreq, 0.0f);

    // 提取窗口并应用 Hann 窗
    auto window = hannWindow(nFFT_);
    std::vector<Complex> fftInput(nFFTWindow_, {0.0f, 0.0f});

    for (int i = 0; i < nFFT_; i++) {
        int idx = frameStart + i;
        if (idx < static_cast<int>(samples.size())) {
            fftInput[i] = {samples[idx] * window[i], 0.0f};
        }
    }

    // 执行 FFT
    fft(fftInput);

    // 计算幅度谱
    for (int k = 0; k < nFreq; k++) {
        magnitude[k] = fftInput[k].magnitudeSq();
    }

    return magnitude;
}

std::vector<float> MelSpectrogram::compute(const std::vector<float>& samples) const {
    int nFreq = nFFTWindow_ / 2 + 1;
    auto filters = melFilterbank();

    // 填充到 30 秒
    int expectedSamples = kWhisperDurationSec * sampleRate_;
    std::vector<float> padded = samples;
    if (static_cast<int>(padded.size()) < expectedSamples) {
        padded.resize(expectedSamples, 0.0f);
    } else if (static_cast<int>(padded.size()) > expectedSamples) {
        padded.resize(expectedSamples);
    }

    // 计算帧数
    int numFrames = nFrames(static_cast<int>(padded.size()));
    if (numFrames <= 0) numFrames = 1;

    // 计算 Mel 频谱图 [nMel x numFrames]
    std::vector<float> melSpec(nMel_ * numFrames, 0.0f);

    for (int t = 0; t < numFrames; t++) {
        int frameStart = t * hopLength_;
        auto magnitude = stft(padded, frameStart);

        // 应用 mel 滤波器组
        for (int m = 0; m < nMel_; m++) {
            float melVal = 0.0f;
            for (int k = 0; k < nFreq; k++) {
                melVal += magnitude[k] * filters[m * nFreq + k];
            }
            // 对数压缩
            melVal = std::max(melVal, 1e-10f);
            melSpec[m * numFrames + t] = std::log(melVal);
        }
    }

    // Whisper 的全局归一化
    float globalMin = melSpec[0];
    for (float v : melSpec) {
        if (v < globalMin) globalMin = v;
    }
    float offset = std::max(globalMin, kMinLevel);
    for (float& v : melSpec) {
        v = (v - offset) / -kMinLevel;
    }

    return melSpec;
}

} // namespace impress
