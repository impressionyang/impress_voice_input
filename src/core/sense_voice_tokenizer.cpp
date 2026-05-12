#include "sense_voice_tokenizer.h"
#include "utils/logger.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

static const char* const kTag = "SenseVoiceTokenizer";

namespace impress {

SenseVoiceTokenizer::SenseVoiceTokenizer() = default;

bool SenseVoiceTokenizer::load(const QString& tokensPath) {
    QFile file(tokensPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(kTag, QString("无法打开词表文件: %1").arg(tokensPath));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    tokenToString_.clear();

    int lineCount = 0;
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;

        // 格式: "<token> <id>" — 最后一个是 token_id
        int lastSpace = line.lastIndexOf(' ');
        if (lastSpace < 0) continue;

        bool ok = false;
        int tokenId = line.mid(lastSpace + 1).toInt(&ok);
        if (!ok) continue;

        QString tokenStr = line.left(lastSpace);
        tokenToString_[tokenId] = tokenStr;
        lineCount++;
    }

    LOG_INFO(kTag, QString("词表已加载: %1 个词条 (%2)").arg(lineCount).arg(tokensPath));
    return !tokenToString_.empty();
}

QString SenseVoiceTokenizer::decode(const std::vector<int>& tokens) const {
    if (tokens.empty()) return "";

    QString result;
    for (int token : tokens) {
        // 跳过特殊 token
        if (token == kTokenBOS || token == kTokenEOS || token == kTokenBlank) {
            continue;
        }

        auto it = tokenToString_.find(token);
        if (it != tokenToString_.end()) {
            QString decoded = decodeBPE(it->second);
            // 过滤 SenseVoice 特殊标签: <|zh|>, <|speech|>, <|NEUTRAL|> 等
            if (decoded.startsWith("<|") && decoded.endsWith("|>")) {
                continue;
            }
            result += decoded;
        } else {
            result += QString("[T%1]").arg(token);
        }
    }

    // 清理首尾空白
    result = result.trimmed();
    // 将多个连续空格合并为单个空格
    result.replace(QRegularExpression("\\s+"), " ");

    return result;
}

QString SenseVoiceTokenizer::decodeBPE(const QString& token) const {
    // SenseVoice 使用 SentencePiece BPE 格式
    // ▁ (U+2581) 表示单词开头/空格
    QString result = token;

    // ▁ → 空格
    result.replace(QChar(0x2581), ' ');

    // 处理 unicode 转义 (如 <0xE5>)
    static QRegularExpression hexPattern("<0x([0-9A-Fa-f]+)>");
    QRegularExpressionMatchIterator it = hexPattern.globalMatch(result);
    QStringList parts;
    int lastPos = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        parts << result.mid(lastPos, match.capturedStart() - lastPos);
        bool ok;
        int code = match.captured(1).toInt(&ok, 16);
        if (ok) {
            parts << QChar(code);
        } else {
            parts << match.captured(0);
        }
        lastPos = match.capturedEnd();
    }
    if (!parts.isEmpty() || lastPos > 0) {
        parts << result.mid(lastPos);
        result = parts.join("");
    }

    return result;
}

} // namespace impress
