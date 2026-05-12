#include "tokenizer.h"
#include "utils/logger.h"
#include <QFile>
#include <QTextStream>

static const char* const kTag = "Tokenizer";

namespace impress {

Tokenizer::Tokenizer() = default;

bool Tokenizer::loadVocabulary(const QString& vocabPath) {
    QFile file(vocabPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(kTag, QString("无法打开词表文件: %1").arg(vocabPath));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    vocabulary_.clear();

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            vocabulary_.push_back(line);
        }
    }

    LOG_INFO(kTag, QString("词表已加载: %1 个词条").arg(vocabulary_.size()));
    return true;
}

QString Tokenizer::decode(const std::vector<int>& tokens) const {
    QString result;
    for (int token : tokens) {
        if (token >= 0 && token < static_cast<int>(vocabulary_.size())) {
            result += vocabulary_[token];
        } else {
            result += QString("<unk:%1>").arg(token);
        }
    }
    return result;
}

} // namespace impress
