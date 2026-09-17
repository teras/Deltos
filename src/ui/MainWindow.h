#pragma once
#include "PageModel.h"
#include "core/DocumentDetector.h"
#include "core/Scan.h"
#include "cli/Cli.h"
#include <QFutureWatcher>
#include <QMainWindow>
#include <memory>

class QListView;
class QComboBox;
class QGraphicsView;
class QGraphicsPixmapItem;
class QLabel;
class QSlider;
class QDoubleSpinBox;
class QAction;
class QToolBar;
class QTimer;

namespace deltos {

class QuadEditorView;
class BusyOverlay;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const Options& opt, QWidget* parent = nullptr);
    void addFiles(const QStringList& paths);
    bool exportPdfTo(const QString& path, QString* error = nullptr, int onlyRow = -1);
    void swapReference();

protected:
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;

private:
    void buildUi();
    void openFiles();
    void currentChanged(int row);
    void redetect();
    void rotateResult();
    void removePage();
    void onQuadEdited(const Quad& q);
    void onModeChanged(int idx);
    void onStrengthChanged(int value);
    void onSizeChoiceChanged(int idx);
    void onManualWidthChanged(double mm);
    void updateSizeRow();
    void setBusy(int delta);   // +1 when a job starts, -1 when it ends
    void applySizeOverride(Page& p);   // recompute effective size from auto + manual, no reprocessing
    void updateStrengthTip();
    void reprocess(int row);
    void showResult(int row);
    void exportPdf(bool currentOnly);
    void exportImage(const QString& format);

    PageModel* model_ = nullptr;
    QListView* list_ = nullptr;
    QuadEditorView* editor_ = nullptr;
    QGraphicsView* resultView_ = nullptr;
    QGraphicsPixmapItem* resultItem_ = nullptr;
    QComboBox* modeBox_ = nullptr;
    QSlider* strength_ = nullptr;
    QLabel* strengthLabel_ = nullptr;
    QTimer* strengthTimer_ = nullptr;
    QLabel* status_ = nullptr;
    QComboBox* sizeBox_ = nullptr;
    QDoubleSpinBox* widthSpin_ = nullptr;
    QLabel* heightLabel_ = nullptr;
    QAction* swapAction_ = nullptr;
    BusyOverlay* editorBusy_ = nullptr;
    BusyOverlay* resultBusy_ = nullptr;
    QWidget* sizeRow_ = nullptr;
    QToolBar* toolbar_ = nullptr;
    int activeJobs_ = 0;

    Options opt_;
    std::unique_ptr<DocumentDetector> detector_;

    struct Job { int row; quint64 gen; Scan scan; QImage thumb; };
    void jobFinished(const Job& j);
    std::vector<quint64> gen_;   // per-page generation counter, latest wins
    int current_ = -1;
};

} // namespace deltos
