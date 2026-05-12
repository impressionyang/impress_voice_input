#pragma once

#include <QTextEdit>

namespace impress {

/**
 * @brief 文本输出控件
 *
 * 带自动滚动和复制功能的文本输出区域。
 */
class TextOutput : public QTextEdit {
    Q_OBJECT
public:
    explicit TextOutput(QWidget* parent = nullptr);

    /** @brief 追加文本并自动滚动到底部 */
    void appendText(const QString& text);

    /** @brief 清空内容 */
    void clearText();

    /** @brief 获取全部文本 */
    QString getFullText() const;
};

} // namespace impress
