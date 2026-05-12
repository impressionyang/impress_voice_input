#include "decoder.h"
#include <algorithm>

namespace impress {

std::vector<int> CTCGreedyDecoder::decode(const std::vector<float>& logits,
                                          int vocabSize,
                                          int /*beamSize*/)
{
    std::vector<int> tokens;
    int prevToken = -1;

    for (size_t t = 0; t < logits.size(); t += vocabSize) {
        // 贪婪选择最大概率的 token
        int bestToken = 0;
        float bestScore = logits[t];
        for (int v = 1; v < vocabSize; ++v) {
            if (logits[t + v] > bestScore) {
                bestScore = logits[t + v];
                bestToken = v;
            }
        }

        // CTC 去重：跳过连续相同 token 和 blank
        if (bestToken != prevToken && bestToken != 0) {
            tokens.push_back(bestToken);
        }
        prevToken = bestToken;
    }

    return tokens;
}

} // namespace impress
