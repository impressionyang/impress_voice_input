#include "whisper_tokenizer.h"
#include "utils/logger.h"
#include <QFile>
#include <QTextStream>

static const char* const kTag = "WhisperTokenizer";

namespace impress {

WhisperTokenizer::WhisperTokenizer() = default;

bool WhisperTokenizer::loadVocabulary(const QString& vocabPath) {
    QFile file(vocabPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(kTag, QString("无法打开词表文件: %1").arg(vocabPath));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    tokenToString_.clear();
    stringToToken_.clear();

    // 支持两种格式：
    // 1. tiktoken base64 格式: "<base64> <token_id>"
    // 2. 纯文本格式: "<token_string> <token_id>"
    int lineCount = 0;
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;

        // 查找最后一个空格分隔 token_id
        int lastSpace = line.lastIndexOf(' ');
        if (lastSpace < 0) continue;

        bool ok = false;
        int tokenId = line.mid(lastSpace + 1).toInt(&ok);
        if (!ok) continue;

        QString tokenStr = line.left(lastSpace);
        tokenToString_[tokenId] = tokenStr;
        stringToToken_[tokenStr] = tokenId;
        lineCount++;
    }

    LOG_INFO(kTag, QString("词表已加载: %1 个词条 (文件: %2)").arg(lineCount).arg(vocabPath));
    return !tokenToString_.empty();
}

QString WhisperTokenizer::decode(const std::vector<int>& tokens) const {
    QString result;
    for (int token : tokens) {
        if (isSpecialToken(token)) continue;

        auto it = tokenToString_.find(token);
        if (it != tokenToString_.end()) {
            QString decoded = decodeBytePair(it->second);
            result += decoded;
        } else {
            result += QString("<|token:%1|>").arg(token);
        }
    }
    return result;
}

std::vector<int> WhisperTokenizer::encode(const QString& text) const {
    std::vector<int> tokens;
    // 简单的字符级编码（实际 BPE 编码需要完整实现）
    for (int i = 0; i < text.length(); i++) {
        QString ch = text.mid(i, 1);
        auto it = stringToToken_.find(ch);
        if (it != stringToToken_.end()) {
            tokens.push_back(it->second);
        }
    }
    return tokens;
}

QString WhisperTokenizer::decodeBytePair(const QString& text) const {
    // Whisper 使用 unicode 转义如 Ġ 表示空格
    QString result = text;
    result.replace(QChar(0x0120), ' ');  // Ġ -> space
    result.replace(QChar(0x010A), '\n');  // Ċ -> newline
    return result;
}

int WhisperTokenizer::languageTokenId(const QString& langCode) {
    static const std::unordered_map<QString, int> langMap = {
        {"zh", 50260}, {"en", 50259}, {"ja", 50261}, {"ko", 50262},
        {"fr", 50265}, {"de", 50266}, {"es", 50267}, {"ru", 50268},
        {"pt", 50269}, {"it", 50270}, {"auto", 50359}
    };
    auto it = langMap.find(langCode);
    return it != langMap.end() ? it->second : 50259; // 默认英语
}

bool WhisperTokenizer::isSpecialToken(int token) {
    // Whisper 特殊 token 范围: [50257, 50362]
    return token >= 50257 && token <= 50363;
}

} // namespace impress
