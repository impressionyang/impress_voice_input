#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <memory>

namespace impress {

/**
 * @brief 录音指示器 — 光标位置浮动窗口
 *
 * 快捷键触发时在鼠标光标位置显示一个小的录音指示器，
 * 使用和系统托盘一致的动态图标（播放三角形 = 录音中）。
 */
class RecordingIndicator : public QWidget {
    Q_OBJECT
public:
    explicit RecordingIndicator(QWidget* parent = nullptr);
    ~RecordingIndicator() override;

    /** @brief 在鼠标位置显示指示器 */
    void showAtCursor();

    /** @brief 隐藏指示器 */
    void hide();

    /** @brief 是否正在显示 */
    bool isVisible() const { return isShowing_; }

private:
    void createUI();
    void updateIcon(bool active);
    QPoint cursorGlobalPosition() const;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool isShowing_ = false;
};

} // namespace impress
