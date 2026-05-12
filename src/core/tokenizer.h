#pragma once

#include <QString>
#include <vector>

namespace impress {

/**
 * @brief Tokenizer
 *
 * 将 token IDs 解码为文本。
 * 支持 BPE (Byte Pair Encoding) 和字符级解码。
 */
class Tokenizer {
public:
    Tokenizer();

    /** @brief 加载词表文件 */
    bool loadVocabulary(const QString& vocabPath);

    /** @brief 将 token IDs 解码为文本 */
    QString decode(const std::vector<int>& tokens) const;

    /** @brief 是否已加载词表 */
    bool isLoaded() const { return !vocabulary_.empty(); }

private:
    std::vector<QString> vocabulary_; // token_id -> token string
};

} // namespace impress
