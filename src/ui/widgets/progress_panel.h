#pragma once

#include <QWidget>

class QProgressBar;
class QLabel;

namespace impress {

/**
 * @brief 进度面板控件
 *
 * 组合进度条和状态标签，用于文件转写等长时间任务。
 */
class ProgressPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProgressPanel(QWidget* parent = nullptr);

    /** @brief 设置进度 (0.0 - 1.0) */
    void setProgress(double value);

    /** @brief 设置状态文字 */
    void setStatusText(const QString& text);

    /** @brief 重置面板 */
    void reset();

    /** @brief 是否可见 */
    bool isActive() const { return isVisible(); }

    /** @brief 显示/隐藏 */
    void show();
    void hide();

private:
    QProgressBar* progressBar_;
    QLabel* statusLabel_;
};

} // namespace impress
