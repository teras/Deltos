#include "OcrTextItem.h"
#include <QApplication>
#include <QClipboard>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>

namespace deltos {

OcrTextItem::OcrTextItem(QGraphicsItem* parent) : QGraphicsObject(parent) {
    setFlag(ItemIsFocusable);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setCursor(Qt::CrossCursor);
    setVisible(false);
}

void OcrTextItem::setWords(const std::vector<Ocr::Word>& words) {
    prepareGeometryChange();
    words_.clear();
    bounds_ = QRectF();
    sweep_ = QRectF();
    dragging_ = false;
    for (const Ocr::Word& w : words) {
        const QRectF box(w.box.x, w.box.y, w.box.width, w.box.height);
        words_.push_back({box, QString::fromStdString(w.text), w.line});
        bounds_ = bounds_.isNull() ? box : bounds_.united(box);
    }
    setVisible(!words_.empty());
    update();
    emit selectionChanged({}, false);
}

void OcrTextItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
    if (words_.empty()) return;
    QPen pen(QColor(0, 120, 215, 160));
    pen.setCosmetic(true);   // one pixel wide whatever the zoom
    p->setPen(pen);
    for (const W& w : words_) {
        p->setBrush(isSelected(w) ? QColor(0, 120, 215, 90) : QColor(0, 120, 215, 25));
        p->drawRect(w.box);
    }
    if (dragging_ && !sweep_.isNull()) {
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(QColor(0, 120, 215), 0, Qt::DashLine));
        p->drawRect(sweep_);
    }
}

int OcrTextItem::selectedCount() const {
    int n = 0;
    for (const W& w : words_) n += isSelected(w);
    return n;
}

// Selected words in layout order (Ocr::run already sorted them that way), with a
// line break wherever the page had one.
QString OcrTextItem::selectedText() const {
    QString out;
    int last = -1;
    for (const W& w : words_) {
        if (!isSelected(w)) continue;
        if (!out.isEmpty()) out += w.line != last ? '\n' : ' ';
        out += w.text;
        last = w.line;
    }
    return out;
}

QString OcrTextItem::allText() const {
    QString out;
    for (size_t i = 0; i < words_.size(); ++i) {
        if (i) out += words_[i].line != words_[i - 1].line ? '\n' : ' ';
        out += words_[i].text;
    }
    return out;
}

void OcrTextItem::copySelection() const {
    const QString text = selectedText();
    if (!text.isEmpty()) QApplication::clipboard()->setText(text);
}

void OcrTextItem::sweepMoved(const QPointF& scenePos) {
    update();
    emit selectionChanged(scenePos, selectedCount() > 0);
}

void OcrTextItem::mousePressEvent(QGraphicsSceneMouseEvent* e) {
    if (e->button() != Qt::LeftButton) { QGraphicsObject::mousePressEvent(e); return; }
    setFocus();
    // A plain click is a zero-sized sweep, which still catches the word under it.
    dragStart_ = e->pos();
    sweep_ = QRectF(dragStart_, dragStart_);
    dragging_ = true;
    sweepMoved(e->scenePos());
    e->accept();
}

void OcrTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e) {
    if (!dragging_) return;
    sweep_ = QRectF(dragStart_, e->pos()).normalized();
    sweepMoved(e->scenePos());
    e->accept();
}

void OcrTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e) {
    if (!dragging_) { QGraphicsObject::mouseReleaseEvent(e); return; }
    dragging_ = false;   // stop drawing the dashed rectangle, keep the words lit
    if (selectedCount() == 0) sweep_ = QRectF();
    sweepMoved(e->scenePos());
    e->accept();
}

void OcrTextItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* e) {
    if (words_.empty()) return;
    QMenu menu;
    QAction* copy = menu.addAction(QObject::tr("Copy"));
    copy->setEnabled(selectedCount() > 0);
    QAction* all = menu.addAction(QObject::tr("Copy all text"));
    const QAction* chosen = menu.exec(e->screenPos());
    if (chosen == copy) copySelection();
    else if (chosen == all) QApplication::clipboard()->setText(allText());
    e->accept();
}

void OcrTextItem::keyPressEvent(QKeyEvent* e) {
    if (e->matches(QKeySequence::Copy)) { copySelection(); e->accept(); }
    else if (e->matches(QKeySequence::SelectAll)) { sweep_ = bounds_; sweepMoved(mapToScene(bounds_.bottomRight())); e->accept(); }
    else QGraphicsObject::keyPressEvent(e);
}

} // namespace deltos
