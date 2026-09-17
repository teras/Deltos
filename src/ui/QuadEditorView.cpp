#include "QuadEditorView.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QMouseEvent>
#include <QPainter>

namespace deltos {

QuadEditorView::QuadEditorView(QWidget* parent) : QGraphicsView(parent) {
    setScene(new QGraphicsScene(this));
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(NoDrag);
    setBackgroundBrush(QColor(40, 40, 40));

    pixmap_ = scene()->addPixmap({});
    poly_ = scene()->addPolygon({}, QPen(QColor(0, 160, 255), 0), QColor(0, 160, 255, 40));
    refPoly_ = scene()->addPolygon({}, QPen(QColor(60, 220, 90), 0, Qt::DashLine), QColor(60, 220, 90, 30));
    refPoly_->setVisible(false);
    for (auto& h : handles_) {
        h = scene()->addEllipse({}, QPen(Qt::white, 0), QColor(0, 160, 255));
        h->setZValue(10);
    }
}

void QuadEditorView::setImage(const QImage& img) {
    pixmap_->setPixmap(QPixmap::fromImage(img));
    scene()->setSceneRect(pixmap_->boundingRect());
    fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
    syncItems();
}

void QuadEditorView::setReferenceQuad(const std::optional<Quad>& q) {
    if (!q) { refPoly_->setVisible(false); return; }
    QPolygonF poly;
    for (int i = 0; i < 4; ++i) poly << QPointF((*q)[i].x, (*q)[i].y);
    refPoly_->setPolygon(poly);
    refPoly_->setVisible(true);
}

void QuadEditorView::setQuad(const Quad& q) {
    quad_ = q;
    syncItems();
}

qreal QuadEditorView::handleRadius() const {
    // Constant size on screen regardless of zoom.
    const qreal scale = transform().m11();
    return scale > 0 ? 8.0 / scale : 8.0;
}

void QuadEditorView::syncItems() {
    QPolygonF poly;
    for (int i = 0; i < 4; ++i) poly << QPointF(quad_[i].x, quad_[i].y);
    poly_->setPolygon(poly);
    const qreal r = handleRadius();
    for (int i = 0; i < 4; ++i)
        handles_[i]->setRect(quad_[i].x - r, quad_[i].y - r, 2 * r, 2 * r);
    viewport()->update();
}

int QuadEditorView::hitHandle(const QPointF& sp) const {
    const qreal r = handleRadius() * 2.5;
    int best = -1;
    qreal bestD = r * r;
    for (int i = 0; i < 4; ++i) {
        const qreal dx = sp.x() - quad_[i].x, dy = sp.y() - quad_[i].y;
        const qreal d = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void QuadEditorView::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        dragging_ = hitHandle(mapToScene(e->pos()));
        if (dragging_ >= 0) { dragPos_ = mapToScene(e->pos()); e->accept(); return; }
    }
    QGraphicsView::mousePressEvent(e);
}

void QuadEditorView::mouseMoveEvent(QMouseEvent* e) {
    if (dragging_ >= 0) {
        const QRectF r = sceneRect();
        QPointF p = mapToScene(e->pos());
        p.setX(std::clamp(p.x(), r.left(), r.right()));
        p.setY(std::clamp(p.y(), r.top(), r.bottom()));
        quad_[dragging_] = cv::Point2f(float(p.x()), float(p.y()));
        dragPos_ = p;
        syncItems();
        e->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(e);
}

void QuadEditorView::mouseReleaseEvent(QMouseEvent* e) {
    if (dragging_ >= 0) {
        dragging_ = -1;
        viewport()->update();
        emit quadEdited(quad_);
        e->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(e);
}

void QuadEditorView::resizeEvent(QResizeEvent* e) {
    QGraphicsView::resizeEvent(e);
    if (!pixmap_->pixmap().isNull()) fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
    syncItems();
}

// Loupe: magnified view around the corner being dragged.
void QuadEditorView::drawForeground(QPainter* p, const QRectF&) {
    if (dragging_ < 0 || pixmap_->pixmap().isNull()) return;
    p->save();
    p->resetTransform();
    const int size = 160, zoom = 3;
    const QPixmap& pm = pixmap_->pixmap();
    const int srcHalf = size / (2 * zoom);
    QRect src(int(dragPos_.x()) - srcHalf, int(dragPos_.y()) - srcHalf, 2 * srcHalf, 2 * srcHalf);
    // Place the loupe in the corner farthest from the cursor.
    QPoint vp = mapFromScene(dragPos_);
    QRect dst(vp.x() < viewport()->width() / 2 ? viewport()->width() - size - 12 : 12,
              vp.y() < viewport()->height() / 2 ? viewport()->height() - size - 12 : 12, size, size);
    p->fillRect(dst.adjusted(-2, -2, 2, 2), Qt::white);
    p->drawPixmap(dst, pm, src);
    p->setPen(QPen(QColor(0, 160, 255), 1.5));
    p->drawLine(dst.center().x() - 12, dst.center().y(), dst.center().x() + 12, dst.center().y());
    p->drawLine(dst.center().x(), dst.center().y() - 12, dst.center().x(), dst.center().y() + 12);
    p->restore();
}

} // namespace deltos
