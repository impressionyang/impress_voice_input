#pragma once

#include <QWidget>
#include <QVector>
#include <vector>

namespace impress {

/**
 * @brief 实时音频波形可视化控件
 */
class AudioWaveform : public QWidget {
    Q_OBJECT
public:
    explicit AudioWaveform(QWidget* parent = nullptr);

    /** @brief 设置音频样本并触发重绘 */
    void setSamples(const std::vector<float>& samples);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<float> samples_;
    QColor lineColor_ = QColor(52, 152, 219);
    QColor fillColor_ = QColor(52, 152, 219, 30);
};

} // namespace impress
