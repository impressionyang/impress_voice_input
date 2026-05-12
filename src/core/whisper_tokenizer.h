#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <unordered_map>
#include <optional>

namespace impress {

/**
 * @brief Whisper Tokenizer
 *
 * 基于 BPE 的分词器，支持 Whisper 模型的 token 编解码。
 * 从 tiktoken 格式的词汇表文件加载。
 */
class WhisperTokenizer {
public:
    WhisperTokenizer();

    /** @brief 从 tiktoken 格式的词汇表文件加载 */
    bool loadVocabulary(const QString& vocabPath);

    /** @brief 将 token IDs 解码为文本 */
    QString decode(const std::vector<int>& tokens) const;

    /** @brief 将文本编码为 token IDs（用于 prompt） */
    std::vector<int> encode(const QString& text) const;

    /** @brief 是否已加载词表 */
    bool isLoaded() const { return !tokenToString_.empty(); }

    /** @brief 词表大小 */
    int vocabSize() const { return static_cast<int>(tokenToString_.size()); }

    // Whisper 特殊 token
    static constexpr int kTokenEndOfText = 50257;
    static constexpr int kTokenEndOfSpeech = 50256;
    static constexpr int kTokenNoSpeech = 50362;
    static constexpr int kTokenTranscription = 50359;

    // 语言 token 起始偏移
    static constexpr int kTokenLanguageBase = 50259;

    /** @brief 获取语言 token ID */
    static int languageTokenId(const QString& langCode);

    /** @brief 判断是否为特殊 token */
    static bool isSpecialToken(int token);

private:
    std::unordered_map<int, QString> tokenToString_;
    std::unordered_map<QString, int> stringToToken_;

    QString decodeBytePair(const QString& text) const;
};

} // namespace impress
