#include "text_output.h"
#include <QScrollBar>

namespace impress {

TextOutput::TextOutput(QWidget* parent)
    : QTextEdit(parent)
{
    setReadOnly(true);
    setFont(QFont("Monospace", 12));
}

void TextOutput::appendText(const QString& text) {
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    setTextCursor(cursor);
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TextOutput::clearText() {
    clear();
}

QString TextOutput::getFullText() const {
    return toPlainText();
}

} // namespace impress
