#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/vad.h"

#include <cmath>
#include <vector>
#include <numeric>

using namespace impress;
using Catch::Matchers::WithinAbs;

// ============================================================================
// VoiceActivityDetector 测试
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

TEST_CASE("静音检测 - 无信号", "[vad]") {
    VoiceActivityDetector vad(16000, 30, 0.02f, 3);
    auto silence = generateSilence(480); // 30ms @ 16kHz

    bool result = vad.process(silence);
    REQUIRE(!result); // 静音
    REQUIRE(!vad.isSpeaking());
}

TEST_CASE("静音检测 - 极低能量", "[vad]") {
    VoiceActivityDetector vad(16000, 30, 0.02f, 3);
    std::vector<float> noise(480, 0.001f); // 极低噪声

    bool result = vad.process(noise);
    REQUIRE(!result); // 应判定为静音
}

TEST_CASE("语音检测 - 纯正弦波", "[vad]") {
    VoiceActivityDetector vad(16000, 30, 0.02f, 3);
    auto tone = generateTone(480, 440.0f, 16000, 0.5f);

    // 需要连续 minVoiceFrames 帧才能判定为语音
    for (int i = 0; i < 2; i++) {
        bool result = vad.process(tone);
        if (i < 2) {
            REQUIRE(!result); // 帧数不足
        }
    }

    // 第 3 帧后应判定为语音
    bool result = vad.process(tone);
    REQUIRE(result); // 连续 3 帧语音
    REQUIRE(vad.isSpeaking());
    REQUIRE(vad.consecutiveVoiceFrames() >= 3);
}

TEST_CASE("语音检测 - 多帧连续", "[vad]") {
    VoiceActivityDetector vad(16000, 30, 0.02f, 3);
    auto tone = generateTone(480, 440.0f, 16000, 0.8f);

    // 连续输入语音帧
    for (int i = 0; i < 10; i++) {
        vad.process(tone);
    }

    REQUIRE(vad.isSpeaking());
    REQUIRE(vad.consecutiveVoiceFrames() == 10);
}

TEST_CASE("语音转静音 - 状态重置", "[vad]") {
    VoiceActivityDetector vad(16000, 30, 0.02f, 3);
    auto tone = generateTone(480, 440.0f, 16000, 0.8f);
    auto silence = generateSilence(480);

    // 先产生语音
    for (int i = 0; i < 5; i++) {
        vad.process(tone);
    }
    REQUIRE(vad.isSpeaking());

    // 然后静音
    vad.process(silence);
    REQUIRE(!vad.isSpeaking());
    REQUIRE(vad.consecutiveVoiceFrames() == 0);
}

TEST_CASE("批量处理 - 静音 + 语音 + 静音", "[vad]") {
    VoiceActivityDetector vad(16000, 30, 0.02f, 3);

    // 90ms 静音 + 300ms 语音 + 90ms 静音
    std::vector<float> samples;
    auto silence1 = generateSilence(1440);
    auto tone = generateTone(4800, 440.0f, 16000, 1.0f); // 更高振幅
    auto silence2 = generateSilence(1440);

    samples.insert(samples.end(), silence1.begin(), silence1.end());
    samples.insert(samples.end(), tone.begin(), tone.end());
    samples.insert(samples.end(), silence2.begin(), silence2.end());

    auto segments = vad.processBatch(samples);

    // 应检测到至少一个语音段
    REQUIRE(!segments.empty());
    REQUIRE(segments[0].startFrame >= 1); // 至少在第一段静音之后
    REQUIRE(segments[0].endFrame > segments[0].startFrame);
}

TEST_CASE("批量处理 - 全静音", "[vad]") {
    VoiceActivityDetector vad(16000, 30, 0.02f, 3);
    auto silence = generateSilence(16000); // 1 秒静音

    auto segments = vad.processBatch(silence);

    REQUIRE(segments.empty());
}

TEST_CASE("能量计算 - 纯正弦波", "[vad]") {
    VoiceActivityDetector vad(16000, 30, 0.02f, 3);
    auto tone = generateTone(480, 440.0f, 16000, 1.0f);

    vad.process(tone);

    // 正弦波能量 ≈ 0.5 (RMS²)
    REQUIRE_THAT(vad.currentEnergy(), WithinAbs(0.5f, 0.01f));
}

TEST_CASE("过零率 - 正弦波", "[vad]") {
    VoiceActivityDetector vad(16000, 30, 0.02f, 3);
    auto tone = generateTone(480, 1000.0f, 16000, 1.0f);

    vad.process(tone);

    // 正弦波每个周期有 2 个过零点
    // 1000Hz 在 16kHz 采样率下：每 16 个样本一个周期，2 个过零点
    // ZCR ≈ 2 * f / sr = 2 * 1000 / 16000 = 0.125
    float expectedZcr = 2.0f * 1000.0f / 16000.0f;
    REQUIRE_THAT(vad.zeroCrossingRate(), WithinAbs(expectedZcr, 0.05f));
}

TEST_CASE("默认构造函数参数", "[vad]") {
    VoiceActivityDetector vad; // 默认参数

    auto silence = generateSilence(480);
    bool result = vad.process(silence);

    REQUIRE(!result); // 默认参数应能正常工作
}
