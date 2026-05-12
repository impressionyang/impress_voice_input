#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/audio_processor.h"

#include <cmath>
#include <numeric>

using namespace impress;
using Catch::Matchers::WithinAbs;

// ============================================================================
// AudioProcessor 测试
// ============================================================================

TEST_CASE("归一化 PCM16 数据", "[audio_processor]") {
    std::vector<short> pcm16 = {0, 16384, -16384, 32767, -32768};

    auto normalized = AudioProcessor::normalize(pcm16);

    REQUIRE(normalized.size() == pcm16.size());
    REQUIRE_THAT(normalized[0], WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(normalized[1], WithinAbs(0.5f, 1e-5f));
    REQUIRE_THAT(normalized[2], WithinAbs(-0.5f, 1e-5f));
    REQUIRE_THAT(normalized[3], WithinAbs(32767.0f / 32768.0f, 1e-5f));
    REQUIRE_THAT(normalized[4], WithinAbs(-1.0f, 1e-5f));
}

TEST_CASE("归一化全零 PCM16 数据", "[audio_processor]") {
    std::vector<short> pcm16(1000, 0);
    auto normalized = AudioProcessor::normalize(pcm16);

    for (float v : normalized) {
        REQUIRE_THAT(v, WithinAbs(0.0f, 1e-5f));
    }
}

TEST_CASE("浮点数据归一化", "[audio_processor]") {
    std::vector<float> input = {0.0f, 0.5f, -0.5f, 1.0f, -1.0f};

    auto normalized = AudioProcessor::normalizeFloats(input);

    // 最大绝对值为 1.0，所以数据不变
    REQUIRE_THAT(normalized[0], WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(normalized[3], WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(normalized[4], WithinAbs(-1.0f, 1e-5f));
}

TEST_CASE("浮点数据归一化 - 小值放大", "[audio_processor]") {
    std::vector<float> input = {0.001f, -0.002f, 0.003f};

    auto normalized = AudioProcessor::normalizeFloats(input);

    // 最大绝对值 0.003，归一化后最大值应为 1.0
    REQUIRE_THAT(normalized[2], WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(normalized[0], WithinAbs(1.0f / 3.0f, 1e-4f));
    REQUIRE_THAT(normalized[1], WithinAbs(-2.0f / 3.0f, 1e-4f));
}

TEST_CASE("等采样率重采样 - 数据不变", "[audio_processor]") {
    AudioProcessor processor(16000);
    std::vector<float> input = {0.0f, 0.5f, -0.5f, 1.0f};

    auto output = processor.resample(input, 16000);

    REQUIRE(output.size() == input.size());
    for (size_t i = 0; i < input.size(); i++) {
        REQUIRE_THAT(output[i], WithinAbs(input[i], 1e-5f));
    }
}

TEST_CASE("上采样 - 样本数增加", "[audio_processor]") {
    AudioProcessor processor(32000);
    std::vector<float> input(1000, 0.5f);

    auto output = processor.resample(input, 16000);

    REQUIRE(output.size() == 2000); // 16k -> 32k = 2x
    // 恒定信号值不变
    REQUIRE_THAT(output[0], WithinAbs(0.5f, 1e-5f));
    REQUIRE_THAT(output[1999], WithinAbs(0.5f, 1e-5f));
}

TEST_CASE("下采样 - 样本数减少", "[audio_processor]") {
    AudioProcessor processor(8000);
    std::vector<float> input(2000, 0.5f);

    auto output = processor.resample(input, 16000);

    REQUIRE(output.size() == 1000); // 16k -> 8k = 0.5x
    REQUIRE_THAT(output[0], WithinAbs(0.5f, 1e-5f));
}

TEST_CASE("分帧 - 基本功能", "[audio_processor]") {
    AudioProcessor processor(16000);
    std::vector<float> input(1000, 1.0f);

    auto frames = processor.frame(input, 200, 100);

    // 帧数: (1000 - 200) / 100 + 1 = 9
    REQUIRE(frames.size() == 9);
    for (const auto& frame : frames) {
        REQUIRE(frame.size() == 200);
    }
}

TEST_CASE("分帧 - 输入不足一帧", "[audio_processor]") {
    AudioProcessor processor(16000);
    std::vector<float> input(50, 1.0f);

    auto frames = processor.frame(input, 200, 100);

    REQUIRE(frames.empty());
}

TEST_CASE("分帧 - 精确匹配", "[audio_processor]") {
    AudioProcessor processor(16000);
    std::vector<float> input(400, 1.0f);

    auto frames = processor.frame(input, 200, 200);

    REQUIRE(frames.size() == 2);
    for (const auto& frame : frames) {
        REQUIRE(frame.size() == 200);
    }
}
