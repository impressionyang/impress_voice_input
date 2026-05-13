#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class QPushButton;

namespace impress {

/**
 * @brief 快捷键录制按钮
 *
 * 点击后进入录制模式，用户按下任意组合键即可设置快捷键。
 * 支持 Ctrl/Alt/Shift/Meta 与字母、数字、功能键的组合。
 */
class HotkeyRecorder : public QWidget {
    Q_OBJECT
public:
    explicit HotkeyRecorder(const QString& label = "快捷键", QWidget* parent = nullptr);
    ~HotkeyRecorder() override;

    /** @brief 获取当前设置的快捷键文本（如 "Ctrl+Alt+K"） */
    QString hotkeyText() const;

    /** @brief 设置快捷键文本 */
    void setHotkeyText(const QString& text);

    /** @brief 是否处于录制模式 */
    bool isRecording() const { return recording_; }

signals:
    void hotkeyChanged(const QString& hotkey);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onToggleRecording();

private:
    void applyStyle();
    void updateDisplay();
    QString hotkeyFromModifiers(int modifiers) const;

    QLabel* label_;
    QPushButton* btn_;
    QString hotkeyText_;
    bool recording_ = false;
};

} // namespace impress
