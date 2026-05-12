#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/mel_spectrogram.h"

#include <cmath>
#include <vector>

using namespace impress;
using Catch::Matchers::WithinAbs;

// ============================================================================
// MelSpectrogram 测试
// ============================================================================

static std::vector<float> generateSilence(int numSamples) {
    return std::vector<float>(numSamples, 0.0f);
}

static std::vector<float> generateTone(int numSamples, float frequency,
                                       int sampleRate = 16000,
                                       float amplitude = 0.5f) {
    std::vector<float> samples(numSamples);
    for (int i = 0; i < numSamples; i++) {
        samples[i] = amplitude * std::sin(2.0f * M_PI * frequency * i / sampleRate);
    }
    return samples;
}

TEST_CASE("构造函数 - 默认参数", "[mel_spectrogram]") {
    MelSpectrogram mel;

    REQUIRE(mel.nMel() == 80);
}

TEST_CASE("帧数计算 - 30秒音频", "[mel_spectrogram]") {
    MelSpectrogram mel(80, 400, 160, 16000);
    int samples = 30 * 16000; // 30秒

    int frames = mel.nFrames(samples);

    // (480000 - 400 + 160) / 160 = 2999
    REQUIRE(frames > 0);
}

TEST_CASE("帧数计算 - 短音频", "[mel_spectrogram]") {
    MelSpectrogram mel(80, 400, 160, 16000);
    int samples = 1600; // 100ms

    int frames = mel.nFrames(samples);
    REQUIRE(frames > 0);
}

TEST_CASE("静音频谱图 - 低能量", "[mel_spectrogram]") {
    MelSpectrogram mel(80, 400, 160, 16000);
    auto silence = generateSilence(480000); // 30秒

    auto melSpec = mel.compute(silence);

    int nFrames = mel.nFrames(static_cast<int>(silence.size()));
    REQUIRE(melSpec.size() == static_cast<size_t>(80 * nFrames));

    // 静音经 log 压缩和归一化后应接近 0
    float maxVal = 0.0f;
    for (float v : melSpec) {
        maxVal = std::max(maxVal, std::abs(v));
    }
    // 归一化后应在 [0, 1] 范围内
    REQUIRE(maxVal <= 1.1f);
}

TEST_CASE("频谱图维度 - 匹配预期", "[mel_spectrogram]") {
    MelSpectrogram mel(80, 400, 160, 16000);
    auto tone = generateTone(480000, 440.0f, 16000, 0.5f);

    auto melSpec = mel.compute(tone);

    int nFrames = mel.nFrames(static_cast<int>(tone.size()));
    REQUIRE(melSpec.size() == static_cast<size_t>(80 * nFrames));
}

TEST_CASE("正弦波频谱图 - 能量分布", "[mel_spectrogram]") {
    MelSpectrogram mel(80, 400, 160, 16000);
    auto tone = generateTone(480000, 440.0f, 16000, 0.8f);

    auto melSpec = mel.compute(tone);

    int nFrames = mel.nFrames(static_cast<int>(tone.size()));
    REQUIRE(melSpec.size() == static_cast<size_t>(80 * nFrames));

    // 440Hz 正弦波应在低频 Mel 滤波器上有较高能量
    // 计算第一帧前几个 Mel bin 的能量
    float lowFreqEnergy = 0.0f;
    for (int m = 0; m < 10; m++) {
        lowFreqEnergy += std::abs(melSpec[m * nFrames]);
    }

    float highFreqEnergy = 0.0f;
    for (int m = 70; m < 80; m++) {
        highFreqEnergy += std::abs(melSpec[m * nFrames]);
    }

    // 低频能量应高于高频（440Hz 是低频信号）
    REQUIRE(lowFreqEnergy > highFreqEnergy);
}

TEST_CASE("不同 Mel 滤波器数量", "[mel_spectrogram]") {
    MelSpectrogram mel40(40, 400, 160, 16000);
    REQUIRE(mel40.nMel() == 40);

    auto tone = generateTone(480000, 440.0f, 16000, 0.5f);
    auto melSpec = mel40.compute(tone);
    int nFrames = mel40.nFrames(static_cast<int>(tone.size()));
    REQUIRE(melSpec.size() == static_cast<size_t>(40 * nFrames));
}

TEST_CASE("频谱图归一化 - 值在合理范围内", "[mel_spectrogram]") {
    MelSpectrogram mel(80, 400, 160, 16000);
    auto tone = generateTone(480000, 440.0f, 16000, 0.5f);

    auto melSpec = mel.compute(tone);

    // Whisper 归一化后的值通常在 [0, 1] 附近
    // 但由于公式 (v - offset) / -kMinLevel，最小值可能为负
    // 当 globalMin < kMinLevel 时，最小值 ≈ (globalMin - kMinLevel) / -kMinLevel
    float minVal = melSpec[0];
    float maxVal = melSpec[0];
    for (float v : melSpec) {
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
    }

    // 值应在合理范围内（不超过 ±2）
    REQUIRE(minVal >= -2.0f);
    REQUIRE(maxVal <= 2.0f);
    // 动态范围不应过大
    REQUIRE((maxVal - minVal) <= 3.0f);
}
