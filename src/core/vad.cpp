#include "vad.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace impress {

VoiceActivityDetector::VoiceActivityDetector(int sampleRate,
                                             int frameMs,
                                             float energyThreshold,
                                             int minVoiceFrames)
    : sampleRate_(sampleRate)
    , frameSize_(sampleRate * frameMs / 1000)
    , energyThreshold_(energyThreshold)
    , minVoiceFrames_(minVoiceFrames)
{}

float VoiceActivityDetector::computeEnergy(const std::vector<float>& samples) const {
    if (samples.empty()) return 0.0f;

    float energy = 0.0f;
    for (float s : samples) {
        energy += s * s;
    }
    return energy / static_cast<float>(samples.size());
}

float VoiceActivityDetector::computeZeroCrossingRate(const std::vector<float>& samples) const {
    if (samples.size() < 2) return 0.0f;

    int crossings = 0;
    for (size_t i = 1; i < samples.size(); i++) {
        if ((samples[i] >= 0.0f) != (samples[i - 1] >= 0.0f)) {
            crossings++;
        }
    }
    return static_cast<float>(crossings) / static_cast<float>(samples.size() - 1);
}

bool VoiceActivityDetector::process(const std::vector<float>& samples) {
    currentEnergy_ = computeEnergy(samples);
    zeroCrossingRate_ = computeZeroCrossingRate(samples);

    // 能量 + 过零率联合判定
    bool isVoice = false;
    if (currentEnergy_ > energyThreshold_) {
        // 高能量 + 低过零率 -> 语音
        if (zeroCrossingRate_ < 0.35f) {
            isVoice = true;
        }
        // 高能量 + 高过零率 -> 可能是摩擦音 /f/ /s/ 等
        else if (zeroCrossingRate_ < 0.5f && currentEnergy_ > energyThreshold_ * 3.0f) {
            isVoice = true;
        }
    }

    // 状态机：连续多帧语音才判定为"正在说话"
    if (isVoice) {
        consecutiveVoiceFrames_++;
    } else {
        consecutiveVoiceFrames_ = 0;
    }

    bool wasSpeaking = isSpeaking_;
    isSpeaking_ = (consecutiveVoiceFrames_ >= minVoiceFrames_);

    return isSpeaking_;
}

std::vector<VoiceActivityDetector::SpeechSegment>
VoiceActivityDetector::processBatch(const std::vector<float>& samples)
{
    std::vector<SpeechSegment> segments;
    if (samples.empty()) return segments;

    // 逐帧处理
    int numSamples = static_cast<int>(samples.size());
    int totalFrames = numSamples / frameSize_;
    if (totalFrames == 0) totalFrames = 1;

    SpeechSegment current;
    current.startFrame = -1;
    bool inSpeech = false;

    for (int f = 0; f < totalFrames; f++) {
        int start = f * frameSize_;
        int end = std::min(start + frameSize_, numSamples);
        std::vector<float> frame(samples.begin() + start, samples.begin() + end);

        bool voice = process(frame);

        if (voice && !inSpeech) {
            // 语音段开始
            current.startFrame = f;
            inSpeech = true;
        } else if (!voice && inSpeech) {
            // 语音段结束
            current.endFrame = f - 1;
            if (current.endFrame - current.startFrame >= minVoiceFrames_) {
                segments.push_back(current);
            }
            inSpeech = false;
            consecutiveVoiceFrames_ = 0;
            isSpeaking_ = false;
        }
    }

    // 如果末尾仍在语音段中
    if (inSpeech) {
        current.endFrame = totalFrames - 1;
        if (current.endFrame - current.startFrame >= minVoiceFrames_) {
            segments.push_back(current);
        }
    }

    return segments;
}

} // namespace impress
