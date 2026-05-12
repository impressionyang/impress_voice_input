#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <unordered_map>

namespace impress {

/**
 * @brief SenseVoice Tokenizer
 *
 * 加载 tokens.txt 词表，支持 BPE token 到文本的解码。
 * 支持 SenseVoice 的 BPE 词表格式 (SentencePiece)。
 */
class SenseVoiceTokenizer {
public:
    SenseVoiceTokenizer();

    /** @brief 从 tokens.txt 加载词表 */
    bool load(const QString& tokensPath);

    /** @brief 将 token IDs 解码为文本 */
    QString decode(const std::vector<int>& tokens) const;

    /** @brief 是否已加载 */
    bool isLoaded() const { return !tokenToString_.empty(); }

    /** @brief 词表大小 */
    int vocabSize() const { return static_cast<int>(tokenToString_.size()); }

    // 特殊 token
    static constexpr int kTokenBlank = 0;     // CTC blank / <unk>
    static constexpr int kTokenBOS = 1;       // <s>
    static constexpr int kTokenEOS = 2;       // </s>

private:
    std::unordered_map<int, QString> tokenToString_;
    QString decodeBPE(const QString& token) const;
};

} // namespace impress
