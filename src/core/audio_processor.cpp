#include "audio_processor.h"
#include <cmath>
#include <algorithm>

namespace impress {

AudioProcessor::AudioProcessor(int targetSampleRate)
    : targetSampleRate_(targetSampleRate)
{}

std::vector<float> AudioProcessor::resample(const std::vector<float>& input,
                                            int sourceSampleRate)
{
    if (sourceSampleRate == targetSampleRate_) {
        return input;
    }

    // TODO: 使用高质量重采样算法 (如 libsamplerate)
    // 当前使用简单的线性插值
    double ratio = static_cast<double>(targetSampleRate_) / sourceSampleRate;
    size_t outputSize = static_cast<size_t>(input.size() * ratio);
    std::vector<float> output(outputSize);

    for (size_t i = 0; i < outputSize; ++i) {
        double srcIndex = i / ratio;
        size_t idx = static_cast<size_t>(srcIndex);
        double frac = srcIndex - idx;

        if (idx + 1 < input.size()) {
            output[i] = input[idx] * (1.0 - frac) + input[idx + 1] * frac;
        } else {
            output[i] = input[idx];
        }
    }

    return output;
}

std::vector<float> AudioProcessor::normalize(const std::vector<short>& pcm16) {
    std::vector<float> output(pcm16.size());
    const float scale = 1.0f / 32768.0f;
    for (size_t i = 0; i < pcm16.size(); ++i) {
        output[i] = pcm16[i] * scale;
    }
    return output;
}

std::vector<float> AudioProcessor::normalizeFloats(const std::vector<float>& input) {
    float maxVal = 0.0f;
    for (float v : input) {
        maxVal = std::max(maxVal, std::abs(v));
    }
    if (maxVal < 1e-6f) return input;

    std::vector<float> output(input.size());
    float scale = 1.0f / maxVal;
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] * scale;
    }
    return output;
}

std::vector<std::vector<float>> AudioProcessor::frame(const std::vector<float>& input,
                                                      int frameSize,
                                                      int hopSize)
{
    std::vector<std::vector<float>> frames;
    for (size_t start = 0; start + frameSize <= input.size(); start += hopSize) {
        frames.emplace_back(input.begin() + start, input.begin() + start + frameSize);
    }
    return frames;
}

} // namespace impress
