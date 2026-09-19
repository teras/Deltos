#pragma once
#include "core/Ocr.h"
#include <QGraphicsObject>
#include <QRectF>
#include <QString>
#include <vector>

namespace deltos {

// Selectable word boxes drawn over the result page, like the text layer of a
// phone scanner. Selection is geometric: dragging sweeps a rectangle and takes
// every word it touches. A range over the reading order would instead pick up
// whatever the recogniser happened to emit between the two ends, which on a form
// can be a paragraph printed somewhere else entirely.
class OcrTextItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit OcrTextItem(QGraphicsItem* parent = nullptr);

    void setWords(const std::vector<Ocr::Word>& words);
    void clear() { setWords({}); }
    bool isEmpty() const { return words_.empty(); }

    void copySelection() const;   // no-op without a selection
    int selectedCount() const;

    QRectF boundingRect() const override { return bounds_; }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

signals:
    // Where the drag currently is, in scene coordinates, so the window can put
    // its Copy button under the mouse. hasSelection is false when the sweep
    // touched no word at all.
    void selectionChanged(const QPointF& sceneEnd, bool hasSelection);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    struct W { QRectF box; QString text; int line; };

    bool isSelected(const W& w) const { return !sweep_.isNull() && sweep_.intersects(w.box); }
    QString selectedText() const;
    QString allText() const;
    void sweepMoved(const QPointF& scenePos);

    std::vector<W> words_;
    QRectF bounds_;
    QRectF sweep_;          // the swept rectangle in item coordinates; null = nothing selected
    QPointF dragStart_;
    bool dragging_ = false;
};

} // namespace deltos
