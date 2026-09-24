#include "waveform_widget.h"

#include <QPaintEvent>
#include <QPainter>

WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(96);
}

void WaveformWidget::setWave(std::vector<PronAudio::WaveColumn> cols) {
    cols_ = std::move(cols);
    update();
}

void WaveformWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), palette().window());

    const int mid = height() / 2;
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawLine(0, mid, width(), mid);

    if (cols_.empty()) {
        return;
    }
    QPen pen(waveColor_);
    pen.setWidthF(1.5);
    pen.setCapStyle(Qt::FlatCap);
    p.setPen(pen);

    const double step = static_cast<double>(width()) / static_cast<double>(cols_.size());
    for (size_t i = 0; i < cols_.size(); ++i) {
        const double x = static_cast<double>(i) * step;
        // int16 满幅映射到半个画布高；min 为负落在中线下方
        const int y1 = mid - cols_[i].max * mid / 32768;
        const int y2 = mid - cols_[i].min * mid / 32768;
        if (y1 == y2) {
            p.drawPoint(QPointF(x, y1));
        } else {
            p.drawLine(QPointF(x, y1), QPointF(x, y2));
        }
    }
}
