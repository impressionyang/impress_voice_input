#include "recording_indicator.h"

#include <QApplication>
#include <QCursor>
#include <QHBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QPainter>
#include <QScreen>
#include <QTimer>

namespace impress {

struct RecordingIndicator::Impl {
    QLabel* iconLabel = nullptr;
    QLabel* textLabel = nullptr;
    QTimer* blinkTimer = nullptr;
    bool iconBlinkState = true;

    /** 创建录音中图标（红色三角形 + 脉冲动画） */
    QPixmap createRecordingIcon(int size) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        const int margin = size / 8;
        const int s = size - margin * 2;

        // 红色圆点（背景脉冲）
        painter.setPen(Qt::NoPen);
        QColor dotColor = iconBlinkState ? QColor(231, 76, 60) : QColor(192, 57, 43);
        painter.setBrush(dotColor);
        painter.drawEllipse(QPoint(size / 2, size / 2), s / 2, s / 2);

        // 白色三角形（播放图标）
        painter.setBrush(Qt::white);
        const int triMargin = size / 3;
        QPolygon triangle;
        triangle << QPoint(triMargin, triMargin)
                 << QPoint(triMargin, size - triMargin)
                 << QPoint(size - triMargin, size / 2);
        painter.drawPolygon(triangle);

        return pixmap;
    }
};

RecordingIndicator::RecordingIndicator(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint)
    , impl_(std::make_unique<Impl>())
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    createUI();
}

RecordingIndicator::~RecordingIndicator() = default;

void RecordingIndicator::createUI() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(6);

    impl_->iconLabel = new QLabel(this);
    impl_->iconLabel->setFixedSize(24, 24);
    layout->addWidget(impl_->iconLabel);

    impl_->textLabel = new QLabel("录音中...", this);
    impl_->textLabel->setStyleSheet(
        "QLabel { color: #e74c3c; font-size: 13px; font-weight: bold; }");
    layout->addWidget(impl_->textLabel);

    // 闪烁动画
    impl_->blinkTimer = new QTimer(this);
    impl_->blinkTimer->setInterval(500);
    connect(impl_->blinkTimer, &QTimer::timeout, this, [this]() {
        impl_->iconBlinkState = !impl_->iconBlinkState;
        updateIcon(true);
    });
}

void RecordingIndicator::updateIcon(bool) {
    if (impl_->iconLabel) {
        impl_->iconLabel->setPixmap(impl_->createRecordingIcon(24));
    }
}

void RecordingIndicator::showAtCursor() {
    if (isShowing_) return;

    QPoint pos = cursorGlobalPosition();

    // 偏移：显示在光标右下方
    pos += QPoint(16, 16);

    // 确保不超出屏幕
    QScreen* screen = QApplication::screenAt(pos);
    if (!screen) screen = QApplication::primaryScreen();

    QRect geo = screen->availableGeometry();
    if (pos.x() + width() > geo.right()) {
        pos.setX(geo.right() - width());
    }
    if (pos.y() + height() > geo.bottom()) {
        pos.setY(geo.bottom() - height());
    }

    move(pos);
    updateIcon(true);
    show();
    isShowing_ = true;

    // 启动闪烁
    impl_->blinkTimer->start();
}

void RecordingIndicator::hide() {
    if (!isShowing_) return;

    impl_->blinkTimer->stop();
    QWidget::hide();
    isShowing_ = false;
}

QPoint RecordingIndicator::cursorGlobalPosition() const {
    return QCursor::pos();
}

} // namespace impress
