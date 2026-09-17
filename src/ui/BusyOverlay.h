#pragma once
#include <QWidget>

class QTimer;

namespace deltos {

// Semi-transparent cover with a spinning arc, placed over a widget while work runs.
class BusyOverlay : public QWidget {
    Q_OBJECT
public:
    explicit BusyOverlay(QWidget* parent);
    void setText(const QString& text) { text_ = text; update(); }

protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;

private:
    QTimer* timer_ = nullptr;
    int angle_ = 0;
    QString text_;
};

} // namespace deltos
