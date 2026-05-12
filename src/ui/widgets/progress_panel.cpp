#include "progress_panel.h"

#include <QVBoxLayout>
#include <QProgressBar>
#include <QLabel>

namespace impress {

ProgressPanel::ProgressPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);

    statusLabel_ = new QLabel("就绪", this);
    layout->addWidget(statusLabel_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    layout->addWidget(progressBar_);

    hide();
}

void ProgressPanel::setProgress(double value) {
    progressBar_->setValue(static_cast<int>(value * 100));
}

void ProgressPanel::setStatusText(const QString& text) {
    statusLabel_->setText(text);
}

void ProgressPanel::reset() {
    progressBar_->setValue(0);
    statusLabel_->setText("就绪");
}

void ProgressPanel::show() {
    QWidget::show();
}

void ProgressPanel::hide() {
    QWidget::hide();
}

} // namespace impress
