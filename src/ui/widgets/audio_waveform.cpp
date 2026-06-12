#include "audio_waveform.h"
#include "app/application.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace impress {

AudioWaveform::AudioWaveform(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(60);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void AudioWaveform::setSamples(const std::vector<float>& samples) {
    samples_.resize(static_cast<int>(samples.size()));
    for (size_t i = 0; i < samples.size(); ++i) {
        samples_[i] = samples[i];
    }
    update();
}

void AudioWaveform::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 根据主题选择背景色
    const QColor bgColor = Application::isDarkTheme()
        ? QColor(42, 42, 42) : QColor(245, 245, 245);

    // 背景
    painter.fillRect(rect(), bgColor);

    if (samples_.isEmpty()) {
        painter.setPen(QColor(180, 180, 180));
        painter.drawText(rect(), Qt::AlignCenter, "无音频数据");
        return;
    }

    int w = width();
    int h = height();
    int centerY = h / 2;
    int maxAmplitude = centerY - 5;

    // 填充区域
    QPainterPath fillPath;
    fillPath.moveTo(0, centerY);

    // 波形线
    QPainterPath linePath;

    for (int x = 0; x < w; ++x) {
        size_t idx = static_cast<size_t>(x) * samples_.size() / w;
        float sample = samples_[idx];
        int y = centerY - static_cast<int>(sample * maxAmplitude);

        fillPath.lineTo(x, y);
        linePath.moveTo(x, y);
    }

    fillPath.lineTo(w, centerY);
    fillPath.closeSubpath();

    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor_);
    painter.drawPath(fillPath);

    painter.setPen(QPen(lineColor_, 1.5));
    painter.drawPath(linePath);
}

} // namespace impress
