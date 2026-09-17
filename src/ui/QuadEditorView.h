#pragma once
#include "core/Quad.h"
#include <QGraphicsView>
#include <array>
#include <optional>

class QGraphicsPixmapItem;
class QGraphicsPolygonItem;
class QGraphicsEllipseItem;

namespace deltos {

// Shows the source photo with four draggable corner handles.
class QuadEditorView : public QGraphicsView {
    Q_OBJECT
public:
    explicit QuadEditorView(QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void setQuad(const Quad& q);
    void setReferenceQuad(const std::optional<Quad>& q);  // green, non-interactive
    Quad quad() const { return quad_; }

signals:
    void quadEdited(const Quad& q);   // emitted on mouse release after a drag

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void drawForeground(QPainter* p, const QRectF& rect) override;

private:
    void syncItems();
    int hitHandle(const QPointF& scenePos) const;
    qreal handleRadius() const;

    QGraphicsPixmapItem* pixmap_ = nullptr;
    QGraphicsPolygonItem* poly_ = nullptr;
    QGraphicsPolygonItem* refPoly_ = nullptr;
    std::array<QGraphicsEllipseItem*, 4> handles_{};
    Quad quad_;
    int dragging_ = -1;
    QPointF dragPos_;
};

} // namespace deltos
