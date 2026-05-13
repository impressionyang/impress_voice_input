#include "hotkey_recorder.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>

namespace impress {

HotkeyRecorder::HotkeyRecorder(const QString& label, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    label_ = new QLabel(label, this);
    layout->addWidget(label_);

    btn_ = new QPushButton(this);
    btn_->setMinimumWidth(140);
    btn_->setFocusPolicy(Qt::StrongFocus);
    connect(btn_, &QPushButton::clicked, this, &HotkeyRecorder::onToggleRecording);
    layout->addWidget(btn_);
    layout->addStretch();

    setHotkeyText("CapsLock");
    applyStyle();
}

HotkeyRecorder::~HotkeyRecorder() = default;

QString HotkeyRecorder::hotkeyText() const {
    return hotkeyText_;
}

void HotkeyRecorder::setHotkeyText(const QString& text) {
    hotkeyText_ = text.isEmpty() ? "未设置" : text;
    recording_ = false;
    updateDisplay();
    applyStyle();
}

void HotkeyRecorder::onToggleRecording() {
    if (recording_) {
        // 取消录制
        recording_ = false;
        updateDisplay();
        applyStyle();
    } else {
        // 进入录制模式
        recording_ = true;
        updateDisplay();
        applyStyle();
        btn_->setFocus();
        btn_->grabKeyboard();
    }
}

bool HotkeyRecorder::eventFilter(QObject* obj, QEvent* event) {
    if (!recording_) return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);

        // 忽略单独的修饰键
        if (ke->key() == Qt::Key_Control || ke->key() == Qt::Key_Alt ||
            ke->key() == Qt::Key_Shift || ke->key() == Qt::Key_Meta) {
            return true;
        }

        // Esc 取消录制
        if (ke->key() == Qt::Key_Escape) {
            recording_ = false;
            updateDisplay();
            applyStyle();
            btn_->releaseKeyboard();
            return true;
        }

        // 构建快捷键文本
        QString text = hotkeyFromModifiers(ke->modifiers());
        if (ke->key() >= Qt::Key_F1 && ke->key() <= Qt::Key_F35) {
            text += QString("F%1").arg(ke->key() - Qt::Key_F1 + 1);
        } else if (ke->key() >= Qt::Key_0 && ke->key() <= Qt::Key_9) {
            text += QString::number(ke->key() - Qt::Key_0);
        } else if (ke->key() >= Qt::Key_A && ke->key() <= Qt::Key_Z) {
            text += QChar(ke->key());
        } else if (ke->key() == Qt::Key_Space) {
            text += "Space";
        } else {
            // 其他按键使用 Qt 名称
            text += QKeySequence(ke->key()).toString();
        }

        hotkeyText_ = text;
        recording_ = false;
        updateDisplay();
        applyStyle();
        btn_->releaseKeyboard();
        emit hotkeyChanged(hotkeyText_);
        return true;
    }

    return QWidget::eventFilter(obj, event);
}

void HotkeyRecorder::updateDisplay() {
    if (recording_) {
        btn_->setText("⏳ 请按键...（Esc 取消）");
    } else {
        btn_->setText(hotkeyText_.isEmpty() ? "未设置" : hotkeyText_);
    }
}

void HotkeyRecorder::applyStyle() {
    if (recording_) {
        btn_->setStyleSheet(
            "QPushButton { background-color: #f39c12; color: white; font-weight: bold; "
            "padding: 6px 12px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #e67e22; }");
    } else {
        btn_->setStyleSheet(
            "QPushButton { background-color: #3498db; color: white; font-weight: bold; "
            "padding: 6px 12px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #2980b9; }");
    }
}

QString HotkeyRecorder::hotkeyFromModifiers(int modifiers) const {
    QString text;
    if (modifiers & Qt::ControlModifier) text += "Ctrl+";
    if (modifiers & Qt::AltModifier) text += "Alt+";
    if (modifiers & Qt::ShiftModifier) text += "Shift+";
    if (modifiers & Qt::MetaModifier) text += "Meta+";
    return text;
}

} // namespace impress
