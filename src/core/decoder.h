#pragma once

#include <QString>
#include <vector>

namespace impress {

/**
 * @brief 解码器接口
 *
 * 支持 CTC 和自回归解码策略。
 */
class Decoder {
public:
    virtual ~Decoder() = default;

    /** @brief 从 logits 解码为 token IDs */
    virtual std::vector<int> decode(const std::vector<float>& logits,
                                    int vocabSize,
                                    int beamSize = 5) = 0;
};

/**
 * @brief CTC 贪婪解码器
 */
class CTCGreedyDecoder : public Decoder {
public:
    std::vector<int> decode(const std::vector<float>& logits,
                            int vocabSize,
                            int beamSize = 5) override;
};

} // namespace impress
