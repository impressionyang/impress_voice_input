#include "sense_voice_features.h"
#include "sense_voice_cmvn.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace impress {

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

static void fft(std::vector<Complex>& x) {
    int n = static_cast<int>(x.size());
    if (n <= 1) return;

    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }

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

static float hzToMel(float hz) {
    return 1125.0f * std::log(1.0f + hz / 700.0f);
}

static float melToHz(float mel) {
    return 700.0f * (std::exp(mel / 1125.0f) - 1.0f);
}

SenseVoiceFeatures::SenseVoiceFeatures(int sampleRate)
    : sampleRate_(sampleRate)
{
    buildMelFilters();
}

std::vector<float> SenseVoiceFeatures::hannWindow() const {
    std::vector<float> window(winLength_);
    for (int i = 0; i < winLength_; i++) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (winLength_ - 1)));
    }
    return window;
}

void SenseVoiceFeatures::buildMelFilters() {
    int nFreq = nFft_ / 2 + 1;
    float fMin = 20.0f;
    float fMax = static_cast<float>(sampleRate_) / 2.0f;
    float melMin = hzToMel(fMin);
    float melMax = hzToMel(fMax);

    std::vector<float> melPoints(nMel_ + 2);
    for (int i = 0; i < nMel_ + 2; i++) {
        melPoints[i] = melToHz(melMin + (melMax - melMin) * i / (nMel_ + 1));
    }

    std::vector<int> binPoints(nMel_ + 2);
    for (int i = 0; i < nMel_ + 2; i++) {
        binPoints[i] = static_cast<int>(std::round((nFft_ + 1) * melPoints[i] / sampleRate_));
        binPoints[i] = std::max(0, std::min(nFreq - 1, binPoints[i]));
    }

    melFilters_.resize(nMel_);
    for (int m = 0; m < nMel_; m++) {
        MelFilter filter;
        filter.startBin = binPoints[m];
        filter.endBin = binPoints[m + 2] + 1;

        int numWeights = filter.endBin - filter.startBin;
        filter.weights.resize(numWeights, 0.0f);

        for (int k = 0; k < numWeights; k++) {
            int bin = filter.startBin + k;
            if (bin >= binPoints[m] && bin <= binPoints[m + 1]) {
                int denom = binPoints[m + 1] - binPoints[m];
                filter.weights[k] = (denom > 0) ? static_cast<float>(bin - binPoints[m]) / denom : 0.0f;
            } else if (bin > binPoints[m + 1] && bin <= binPoints[m + 2]) {
                int denom = binPoints[m + 2] - binPoints[m + 1];
                filter.weights[k] = (denom > 0) ? static_cast<float>(binPoints[m + 2] - bin) / denom : 0.0f;
            }
        }

        melFilters_[m] = filter;
    }
}

int SenseVoiceFeatures::nFrames(int numSamples) const {
    if (numSamples < winLength_) return 0;
    return (numSamples - winLength_) / hopLength_ + 1;
}

std::vector<float> SenseVoiceFeatures::extract(const std::vector<float>& samples) const {
    if (samples.empty()) return {};

    int numSamples = static_cast<int>(samples.size());

    // 1. 预加重
    std::vector<float> emphasized(numSamples);
    emphasized[0] = samples[0];
    for (int i = 1; i < numSamples; i++) {
        emphasized[i] = samples[i] - preEmphasisCoeff_ * samples[i - 1];
    }

    // 2. 分帧 + FFT + Mel + 对数压缩
    int numFrames = nFrames(numSamples);
    if (numFrames <= 0) return {};

    auto window = hannWindow();
    int nFreq = nFft_ / 2 + 1;

    std::vector<float> fbankData(numFrames * nMel_);

    for (int f = 0; f < numFrames; f++) {
        int frameStart = f * hopLength_;

        // 应用 Hann 窗并 FFT
        std::vector<Complex> fftInput(nFft_, {0.0f, 0.0f});
        for (int i = 0; i < winLength_ && frameStart + i < numSamples; i++) {
            fftInput[i] = {emphasized[frameStart + i] * window[i], 0.0f};
        }
        fft(fftInput);

        // Mel 滤波器组
        for (int m = 0; m < nMel_; m++) {
            const auto& filter = melFilters_[m];
            float energy = 0.0f;
            for (int w = 0; w < static_cast<int>(filter.weights.size()); w++) {
                int bin = filter.startBin + w;
                if (bin < nFreq) {
                    energy += fftInput[bin].magnitudeSq() * filter.weights[w];
                }
            }
            // 对数压缩 (使用自然对数)
            energy = std::max(energy, 1e-10f);
            fbankData[f * nMel_ + m] = std::log(energy);
        }
    }

    // 3. LFR (Low Frame Rate) 特征拼接
    //    将连续 lfr_window_size 帧 Fbank 特征拼接为一帧
    //    步长为 lfr_window_shift
    std::vector<float> lfrFeatures;
    int lfrOutputDim = nMel_ * kLFRWindowSize;  // 80 * 7 = 560

    for (int i = 0; ; i += kLFRWindowShift) {
        if (i >= numFrames) break;

        // 计算 LFR 窗口
        int leftPad = std::max(0, kLFRWindowSize / 2 - i);
        int rightPad = std::max(0, kLFRWindowSize / 2 - (numFrames - 1 - i));

        std::vector<float> frame(lfrOutputDim, 0.0f);
        int outIdx = 0;

        for (int j = -kLFRWindowSize / 2; j < kLFRWindowSize - kLFRWindowSize / 2; j++) {
            int idx = i + j;
            // 边界填充：复制第一帧或最后一帧
            if (idx < 0) idx = 0;
            if (idx >= numFrames) idx = numFrames - 1;

            for (int m = 0; m < nMel_; m++) {
                frame[outIdx++] = fbankData[idx * nMel_ + m];
            }
        }

        lfrFeatures.insert(lfrFeatures.end(), frame.begin(), frame.end());
    }

    // 4. CMVN 归一化
    cmvn(lfrFeatures);

    return lfrFeatures;
}

void SenseVoiceFeatures::cmvn(std::vector<float>& features) const {
    int nLFRFrames = static_cast<int>(features.size()) / kLFROutputDim;
    int numValues = static_cast<int>(features.size());

    for (int i = 0; i < numValues; i++) {
        features[i] = (features[i] + kNegMean[i % kLFROutputDim]) *
                      kInvStddev[i % kLFROutputDim];
    }
}

} // namespace impress
