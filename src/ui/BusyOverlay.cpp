#include "BusyOverlay.h"
#include <QEvent>
#include <QPainter>
#include <QTimer>

namespace deltos {

BusyOverlay::BusyOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);   // swallow clicks
    setAttribute(Qt::WA_NoSystemBackground);
    parent->installEventFilter(this);
    resize(parent->size());
    timer_ = new QTimer(this);
    timer_->setInterval(40);
    connect(timer_, &QTimer::timeout, this, [this] { angle_ = (angle_ + 12) % 360; update(); });
    hide();
}

bool BusyOverlay::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == parent() && ev->type() == QEvent::Resize) resize(parentWidget()->size());
    return QWidget::eventFilter(obj, ev);
}

void BusyOverlay::showEvent(QShowEvent*) { raise(); timer_->start(); }
void BusyOverlay::hideEvent(QHideEvent*) { timer_->stop(); }

void BusyOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0, 0, 0, 110));

    const int r = 28;
    const QPointF c(width() / 2.0, height() / 2.0);
    QPen pen(QColor(255, 255, 255, 60), 5, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    p.drawEllipse(c, r, r);
    pen.setColor(QColor(0, 160, 255));
    p.setPen(pen);
    p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), -angle_ * 16, 100 * 16);

    if (!text_.isEmpty()) {
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPointSizeF(f.pointSizeF() * 1.1);
        p.setFont(f);
        p.drawText(QRectF(0, c.y() + r + 10, width(), 30), Qt::AlignHCenter | Qt::AlignTop, text_);
    }
}

} // namespace deltos
