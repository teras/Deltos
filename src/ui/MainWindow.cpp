#include "MainWindow.h"
#include "LanguageDialog.h"
#include "QuadEditorView.h"
#include "BusyOverlay.h"
#include "OcrTextItem.h"
#include "core/CvQt.h"
#include "core/ImageLoader.h"
#include "core/Ocr.h"
#include "core/Settings.h"
#include "core/Scan.h"
#include "export/ImageExporter.h"
#include "QrImage.h"
#include <cmath>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QImageReader>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QMimeData>
#include <QIcon>
#include <QLocale>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QTimer>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <opencv2/imgproc.hpp>

namespace deltos {

MainWindow::MainWindow(const Options& opt, QWidget* parent) : QMainWindow(parent), opt_(opt) {
    setWindowTitle("Deltos");
    resize(1300, 800);
    setAcceptDrops(true);

    model_ = new PageModel(this);
    detector_ = std::make_unique<DocumentDetector>(opt_.model);
    detector_->modelWantsRgb = opt_.rgb;

    buildUi();
    status_->setText(detector_->hasModel() ? tr("Model: %1").arg(QString::fromStdString(detector_->modelName()))
                                           : tr("No model found – using edge detection only"));

    receiver_ = new LocalSendReceiver(this);
    connect(receiver_, &LocalSendReceiver::fileReceived, this, [this](const QString& path) { addFiles({path}); });
    phoneAction_->setChecked(QSettings(settingsPath(), QSettings::IniFormat).value("receiver/enabled", false).toBool());

}

void MainWindow::jobFinished(const Job& j) {
    if (j.row >= model_->rowCount() || j.row >= int(gen_.size()) || gen_[size_t(j.row)] != j.gen) return;
    Page& p = model_->page(j.row);
    const Scan& s = j.scan;
    p.result = s.image;
    p.rectified = s.rectified;
    p.ocr = {};
    p.ocrDone = false;
    p.thumbnail = j.thumb;
    p.autoRotation = s.autoRotation;
    p.autoWidthMm = s.widthMm;
    p.autoSizeSource = int(s.sizeSource);
    p.measuredBy = QString::fromLatin1(s.measuredBy);
    applySizeOverride(p);
    p.standard = QString::fromLatin1(s.standard);
    p.refQuad = s.refQuad;
    p.refWidthMm = s.refWidthMm;
    p.swappable = s.swappable;
    if (j.row == current_) {
        editor_->setReferenceQuad(p.refQuad);
        swapAction_->setVisible(p.swappable);
    }
    if (!p.detected) {
        p.detected = true;
        p.quad = s.quad;
        p.detection = QString::fromStdString(s.detection);
        if (j.row == current_) {
            editor_->setQuad(p.quad);
            status_->setText(tr("Detection: %1 (%2%)").arg(p.detection).arg(int(s.confidence * 100)));
        }
    }
    model_->pageChanged(j.row);
    if (j.row == current_) {
        showResult(j.row);
        if (opt_.ocr) runOcr();   // --ocr reads the page in view as soon as it is ready
    }
}

void MainWindow::buildUi() {
    auto* tb = addToolBar(tr("Main"));
    toolbar_ = tb;
    tb->setMovable(false);
    auto* openMenu = new QMenu(this);
    openMenu->addAction(tr("Open files…"), QKeySequence::Open, this, &MainWindow::openFiles);
    phoneAction_ = openMenu->addAction(tr("Receive from phone"));
    phoneAction_->setCheckable(true);
    connect(phoneAction_, &QAction::toggled, this, &MainWindow::setReceiving);
    qrAction_ = openMenu->addAction(tr("QR code…"), this, &MainWindow::showPhoneQr);
    qrAction_->setEnabled(false);
    auto* openButton = new QToolButton;
    openButton->setText(tr("Open"));
    openButton->setMenu(openMenu);
    openButton->setPopupMode(QToolButton::InstantPopup);
    tb->addWidget(openButton);
    tb->addAction(tr("Detect again"), this, &MainWindow::redetect);
    tb->addAction(tr("Rotate 90°"), this, &MainWindow::rotateResult);
    tb->addAction(tr("Remove page"), QKeySequence::Delete, this, &MainWindow::removePage);
    swapAction_ = tb->addAction(tr("Swap document ↔ reference"), this, &MainWindow::swapReference);
    swapAction_->setToolTip(tr("Two card-shaped objects were found: use the other one as the document"));
    swapAction_->setVisible(false);
    tb->addSeparator();
    tb->addWidget(new QLabel(tr(" Mode: ")));
    modeBox_ = new QComboBox;
    modeBox_->addItems({tr("Color"), tr("Grayscale"), tr("Black & white")});
    connect(modeBox_, &QComboBox::currentIndexChanged, this, &MainWindow::onModeChanged);
    tb->addWidget(modeBox_);
    strengthLabel_ = new QLabel;
    tb->addWidget(strengthLabel_);
    strength_ = new QSlider(Qt::Horizontal);
    strength_->setRange(0, 100);
    strength_->setValue(50);
    strength_->setFixedWidth(160);
    strength_->setTickPosition(QSlider::TicksBelow);
    strength_->setTickInterval(25);
    connect(strength_, &QSlider::valueChanged, this, &MainWindow::onStrengthChanged);
    tb->addWidget(strength_);
    strengthTimer_ = new QTimer(this);
    strengthTimer_->setSingleShot(true);
    strengthTimer_->setInterval(150);
    connect(strengthTimer_, &QTimer::timeout, this, [this] { if (current_ >= 0) reprocess(current_); });
    updateStrengthTip();
    auto* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);
    auto* exportMenu = new QMenu(this);
    exportMenu->addAction(tr("PDF, all pages…"), QKeySequence::Save, this, [this] { exportPdf(false); });
    exportMenu->addAction(tr("PDF, current page…"), this, [this] { exportPdf(true); });
    exportMenu->addAction(tr("PNG, current page…"), this, [this] { exportImage("png"); });
    exportMenu->addAction(tr("JPG, current page…"), this, [this] { exportImage("jpg"); });
    auto* exportButton = new QToolButton;
    exportButton->setText(tr("Export"));
    exportButton->setMenu(exportMenu);
    exportButton->setPopupMode(QToolButton::InstantPopup);
    tb->addWidget(exportButton);

    list_ = new QListView;
    list_->setModel(model_);
    list_->setIconSize({140, 140});
    list_->setSpacing(4);
    list_->setDragDropMode(QAbstractItemView::InternalMove);
    list_->setDefaultDropAction(Qt::MoveAction);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setMinimumWidth(200);
    connect(list_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& c) { currentChanged(c.isValid() ? c.row() : -1); });

    editor_ = new QuadEditorView;
    connect(editor_, &QuadEditorView::quadEdited, this, &MainWindow::onQuadEdited);

    resultView_ = new QGraphicsView(new QGraphicsScene(this));
    resultView_->setBackgroundBrush(QColor(60, 60, 60));
    resultView_->setRenderHints(QPainter::SmoothPixmapTransform);
    resultItem_ = resultView_->scene()->addPixmap({});
    ocrItem_ = new OcrTextItem(resultItem_);   // child: follows the page pixmap
    copyButton_ = new QPushButton(resultView_->viewport());
    const QIcon copyIcon = QIcon::fromTheme("edit-copy");
    if (copyIcon.isNull()) copyButton_->setText(tr("Copy"));   // no icon theme: fall back to a label
    else copyButton_->setIcon(copyIcon);
    copyButton_->setToolTip(tr("Copy the selected text (Ctrl+C)"));
    copyButton_->hide();
    connect(copyButton_, &QPushButton::clicked, this, [this] {
        ocrItem_->copySelection();
        status_->setText(tr("%n word(s) copied", nullptr, ocrItem_->selectedCount()));
    });
    connect(ocrItem_, &OcrTextItem::selectionChanged, this, &MainWindow::placeCopyButton);

    sizeBox_ = new QComboBox;
    sizeBox_->addItems({tr("Auto"), "A4", "A5", "A3", "Letter", "Legal", "ID card", "DL", tr("Custom")});
    connect(sizeBox_, &QComboBox::currentIndexChanged, this, &MainWindow::onSizeChoiceChanged);
    widthSpin_ = new QDoubleSpinBox;
    widthSpin_->setRange(5, 2000);
    widthSpin_->setDecimals(1);
    widthSpin_->setSuffix(" mm");
    widthSpin_->setReadOnly(true);                       // read-only keeps normal text colour, unlike disabled
    widthSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    connect(widthSpin_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onManualWidthChanged);
    heightLabel_ = new QLabel;
    auto* sizeRow = new QWidget;
    sizeRow_ = sizeRow;
    auto* sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setContentsMargins(6, 4, 6, 4);
    sizeLayout->addWidget(new QLabel(tr("Size:")));
    sizeLayout->addWidget(sizeBox_);
    sizeLayout->addWidget(widthSpin_);
    sizeLayout->addWidget(heightLabel_);
    sizeLayout->addStretch(1);
    ocrLangButton_ = new QToolButton;
    ocrLangButton_->setPopupMode(QToolButton::InstantPopup);
    ocrLangMenu_ = new QMenu(this);
    ocrLangButton_->setMenu(ocrLangMenu_);
    buildLanguageMenu();
    sizeLayout->addWidget(ocrLangButton_);
    ocrButton_ = new QPushButton(tr("OCR"));
    ocrButton_->setToolTip(tr("Recognise the text of this page and show it as selectable boxes"));
    connect(ocrButton_, &QPushButton::clicked, this, &MainWindow::runOcr);
    sizeLayout->addWidget(ocrButton_);
    auto* resultBox = new QWidget;
    auto* resultLayout = new QVBoxLayout(resultBox);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    resultLayout->setSpacing(0);
    resultLayout->addWidget(sizeRow);
    resultLayout->addWidget(resultView_, 1);

    auto* right = new QSplitter(Qt::Horizontal);
    right->addWidget(editor_);
    right->addWidget(resultBox);
    right->setStretchFactor(0, 1);
    right->setStretchFactor(1, 1);
    right->setSizes({500, 500});

    auto* root = new QSplitter(Qt::Horizontal);
    root->addWidget(list_);
    root->addWidget(right);
    root->setStretchFactor(0, 0);
    root->setStretchFactor(1, 1);
    setCentralWidget(root);

    status_ = new QLabel;
    statusBar()->addWidget(status_, 1);
    receiveStatus_ = new QLabel;
    statusBar()->addPermanentWidget(receiveStatus_);

    editorBusy_ = new BusyOverlay(editor_);
    resultBusy_ = new BusyOverlay(resultView_);
    resultBusy_->setText(tr("Processing…"));
    updateOcrRow();
}

void MainWindow::setReceiving(bool on) {
    QSettings(settingsPath(), QSettings::IniFormat).setValue("receiver/enabled", on);
    if (on && !receiver_->isRunning() && !receiver_->start()) {
        phoneAction_->setChecked(false);
        QMessageBox::warning(this, tr("Receive from phone"), tr("Could not open a port for receiving."));
        return;
    }
    if (!on) receiver_->stop();
    qrAction_->setEnabled(on);
    receiveStatus_->setText(on ? tr("Receiving on port %1 ").arg(receiver_->port()) : QString());
    receiveStatus_->setToolTip(on ? tr("LocalSend app → \"%1\", browser → %2").arg(receiver_->alias(), receiver_->url()) : QString());
}

// QR code of the upload page, for the phone's camera.
void MainWindow::showPhoneQr() {
    const QString url = receiver_->url();
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Receive from phone"));
    auto* layout = new QVBoxLayout(&dlg);
    auto* pic = new QLabel;
    pic->setPixmap(QPixmap::fromImage(qrImage(url)));
    pic->setAlignment(Qt::AlignCenter);
    layout->addWidget(pic);
    auto* text = new QLabel(tr("<p>Scan with the phone's camera, or open<br><b>%1</b><br>in its browser and pick the photos.</p>"
                               "<p>With the LocalSend app, send to <b>%2</b>.</p>"
                               "<p style='color:gray'>Port %3 must be open in this computer's firewall.</p>")
                                .arg(url, receiver_->alias()).arg(receiver_->port()));
    text->setAlignment(Qt::AlignCenter);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(text);
    dlg.exec();
}

void MainWindow::setBusy(int delta) {
    activeJobs_ += delta;
    const bool busy = activeJobs_ > 0;
    toolbar_->setEnabled(!busy);
    list_->setEnabled(!busy);
    sizeRow_->setEnabled(!busy);
    editor_->setInteractive(!busy);
    editorBusy_->setVisible(busy);
    resultBusy_->setVisible(busy);
    setAcceptDrops(!busy);
    updateOcrRow();
}

void MainWindow::openFiles() {
    QStringList filters;
    for (const QByteArray& f : QImageReader::supportedImageFormats()) filters << "*." + QString::fromLatin1(f);
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Open images"), {},
        tr("Images (%1)").arg(filters.join(' ')));
    addFiles(files);
}

void MainWindow::addFiles(const QStringList& paths) {
    int firstRow = -1;
    for (const QString& path : paths) {
        LoadedImage li = loadImage(path.toStdString());
        if (li.bgr.empty()) {
            QMessageBox::warning(this, tr("Open"), tr("Cannot read %1: %2").arg(path, QString::fromStdString(li.error)));
            continue;
        }
        Page p;
        p.sourcePath = path;
        p.source = li.bgr;
        p.focalPx = opt_.process.focalPx == 0 ? li.focalPx : opt_.process.focalPx;
        p.subjectDistanceMm = li.subjectDistanceMm;
        p.mode = opt_.process.mode;
        p.strength = int(std::lround(opt_.process.strength * 100));
        p.quad = Quad::fromImage(p.source.size());  // until detection runs in the background
        const int row = model_->addPage(std::move(p));
        gen_.push_back(0);
        if (firstRow < 0) firstRow = row;
        reprocess(row);
    }
    if (firstRow >= 0) list_->setCurrentIndex(model_->index(firstRow));
}

void MainWindow::currentChanged(int row) {
    current_ = row;
    if (row < 0) {
        editor_->setImage({});
        resultItem_->setPixmap({});
        ocrItem_->clear();
        heightLabel_->clear();
        updateOcrRow();
        return;
    }
    const Page& p = model_->page(row);
    editor_->setImage(matToQImage(p.source));
    editor_->setQuad(p.quad);
    editor_->setReferenceQuad(p.refQuad);
    swapAction_->setVisible(p.swappable);
    modeBox_->blockSignals(true);
    modeBox_->setCurrentIndex(int(p.mode));
    modeBox_->blockSignals(false);
    strength_->blockSignals(true);
    strength_->setValue(p.strength);
    strength_->blockSignals(false);
    updateStrengthTip();
    showResult(row);
    // The status line belongs to the page in view: the word count of the page we
    // just left, still sitting there, reads as if this one had been recognised.
    if (p.ocrDone)
        status_->setText(p.ocr.words.empty() ? tr("No text found")
                                             : tr("%n word(s) recognised", nullptr, int(p.ocr.words.size())));
    else if (!p.detection.isEmpty())
        status_->setText(tr("Detection: %1").arg(p.detection));   // confidence belongs to the run, not the page
    else
        status_->clear();
}

void MainWindow::showResult(int row) {
    updateSizeRow();
    const Page& p = model_->page(row);
    resultItem_->setPixmap(QPixmap::fromImage(matToQImage(p.result)));
    resultView_->scene()->setSceneRect(resultItem_->boundingRect());
    resultView_->fitInView(resultView_->scene()->sceneRect(), Qt::KeepAspectRatio);
    ocrItem_->setWords(p.ocr.words);
    updateOcrRow();
}

// The Copy button follows the end of the sweep, just clear of the pointer, like
// the one a phone pops up.
void MainWindow::placeCopyButton(const QPointF& sceneEnd, bool hasSelection) {
    if (!hasSelection) { copyButton_->hide(); return; }
    const QPoint end = resultView_->mapFromScene(sceneEnd);
    const QSize s = copyButton_->sizeHint();
    const int w = resultView_->viewport()->width(), h = resultView_->viewport()->height();
    int x = end.x() + 8, y = end.y() + 8;
    if (x + s.width() > w) x = end.x() - s.width() - 8;    // flip to the other side
    if (y + s.height() > h) y = end.y() - s.height() - 8;
    x = std::clamp(x, 0, std::max(0, w - s.width()));
    y = std::clamp(y, 0, std::max(0, h - s.height()));
    copyButton_->setGeometry(QRect(QPoint(x, y), s));
    copyButton_->show();
    copyButton_->raise();
}

// Tesseract names its files by ISO 639-2 code; show what those codes mean, and
// fall back to the bare code for the ones Qt does not know (chi_sim, equ, ...).
static QString languageName(const QString& code) {
    QLocale::Language lang = QLocale::AnyLanguage;
    for (auto type : {QLocale::ISO639Part2T, QLocale::ISO639Part2B, QLocale::ISO639Part3})
        if ((lang = QLocale::codeToLanguage(code, type)) != QLocale::AnyLanguage) break;
    return lang == QLocale::AnyLanguage ? code
                                        : QString("%1 (%2)").arg(QLocale::languageToString(lang), code);
}

// One tickable entry per installed traineddata: Tesseract happily takes several
// at once, and a scan is often mixed (a Greek bill still prints Latin codes).
void MainWindow::buildLanguageMenu() {
    // Rebuilt whenever a language is added or removed, so start from what is
    // ticked now and fall back to the saved setting the first time round.
    QStringList chosen = ocrOrder_.isEmpty()
                             ? QString::fromStdString(opt_.ocrLanguage).split('+', Qt::SkipEmptyParts)
                             : ocrOrder_;
    ocrLangMenu_->clear();
    ocrOrder_.clear();
    for (const std::string& code : Ocr::availableLanguages()) {
        if (code == "osd") continue;   // orientation data, not a recognition language
        const QString c = QString::fromStdString(code);
        QAction* a = ocrLangMenu_->addAction(languageName(c));
        a->setCheckable(true);
        a->setData(c);
    }
    // Whatever was ticked may have just been removed from the disk; recognising
    // in nothing is not a state, so fall back to what the system suggests.
    chosen.removeIf([this](const QString& c) { return !installedLanguage(c); });
    if (chosen.isEmpty())
        chosen = QString::fromStdString(Ocr::defaultLanguage()).split('+', Qt::SkipEmptyParts);
    // The menu is alphabetical, the spec is not: Tesseract treats the first
    // language as the primary one, so keep the order the user ticked them in
    // (and the saved order when it comes from the settings).
    for (const QString& c : chosen)
        if (installedLanguage(c)) ocrOrder_ << c;
    for (QAction* a : ocrLangMenu_->actions())
        a->setChecked(ocrOrder_.contains(a->data().toString()));

    // Connect only now: ticking the initial state above must not look like a
    // choice, or the first run would save a setting the user never made and the
    // language would stop following the system from then on.
    for (QAction* a : ocrLangMenu_->actions()) {
        connect(a, &QAction::toggled, this, [this, a](bool on) {
            const QString code = a->data().toString();
            if (on) {
                if (!ocrOrder_.contains(code)) ocrOrder_ << code;
            } else if (ocrOrder_.size() < 2) {
                a->setChecked(true);   // keep at least one; re-enters here with on == true
                return;
            } else {
                ocrOrder_.removeAll(code);
            }
            if (ocrLanguage() == opt_.ocrLanguage) return;   // the revert above changed nothing
            opt_.ocrLanguage = ocrLanguage();
            QSettings(settingsPath(), QSettings::IniFormat)
                .setValue("ocr/language", QString::fromStdString(opt_.ocrLanguage));
            updateOcrRow();
            if (current_ >= 0 && model_->page(current_).ocrDone) runOcr();   // redo what is on screen
        });
    }
    ocrLangMenu_->addSeparator();
    connect(ocrLangMenu_->addAction(tr("Manage languages…")), &QAction::triggered, this, [this] {
        LanguageDialog(this).exec();
        // Queued, because rebuilding deletes the very action this runs from.
        QMetaObject::invokeMethod(
            this, [this] { buildLanguageMenu(); updateOcrRow(); }, Qt::QueuedConnection);
    });
    ocrLangButton_->setVisible(true);
}

bool MainWindow::installedLanguage(const QString& code) const {
    for (const QAction* a : ocrLangMenu_->actions())
        if (a->data().toString() == code) return true;
    return false;
}

std::string MainWindow::ocrLanguage() const {
    return ocrOrder_.join('+').toStdString();
}

void MainWindow::updateOcrRow() {
    const bool ready = current_ >= 0 && !model_->page(current_).result.empty();
    ocrButton_->setEnabled(ready && !ocrRunning_ && activeJobs_ == 0);
    ocrButton_->setText(ocrRunning_ ? tr("Reading…") : tr("OCR"));

    QStringList picked;
    for (const QString& code : ocrOrder_) picked << languageName(code);
    ocrLangButton_->setText(picked.isEmpty() ? tr("Language")
                            : picked.size() == 1 ? picked.first()
                                                 : tr("%1 +%2").arg(picked.first()).arg(picked.size() - 1));
    ocrLangButton_->setToolTip(tr("Recognise the text in: %1").arg(picked.join(", ")));
}

// Optional extra Tesseract pass on the page in view. It reads the un-enhanced
// rectified image, so the result survives later mode/strength changes -- but any
// reprocessing drops it anyway, because the quad or the rotation may have moved.
void MainWindow::runOcr() {
    if (current_ < 0 || ocrRunning_) return;
    Page& p = model_->page(current_);
    if (p.rectified.empty()) return;
    const int row = current_;
    const quint64 gen = gen_[size_t(row)];
    const cv::Mat src = p.rectified;
    const std::string lang = ocrLanguage().empty() ? opt_.ocrLanguage : ocrLanguage();

    ocrRunning_ = true;
    updateOcrRow();
    auto* watcher = new QFutureWatcher<Ocr::Result>(this);
    connect(watcher, &QFutureWatcher<Ocr::Result>::finished, this, [this, watcher, row, gen] {
        ocrRunning_ = false;
        if (row < model_->rowCount() && row < int(gen_.size()) && gen_[size_t(row)] == gen) {
            Page& p = model_->page(row);
            p.ocr = watcher->result();
            p.ocrDone = true;
            if (row == current_) {
                ocrItem_->setWords(p.ocr.words);
                status_->setText(p.ocr.words.empty() ? tr("No text found")
                                                     : tr("%n word(s) recognised", nullptr, int(p.ocr.words.size())));
            }
        }
        updateOcrRow();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([src, lang] { return Ocr::run(src, lang); }));
}

void MainWindow::reprocess(int row) {
    Page& p = model_->page(row);
    const quint64 gen = ++gen_[size_t(row)];
    ScanInput in;
    in.source = p.source;
    in.options = opt_.process;
    in.options.mode = p.mode;
    in.options.strength = p.strength / 100.0;
    in.options.focalPx = p.focalPx;
    in.options.subjectDistanceMm = p.subjectDistanceMm;
    if (p.detected) in.quad = p.quad;
    in.refQuad = p.refQuad;
    in.autoRotation = p.autoRotation;
    in.userRotation = p.rotation;
    DocumentDetector* det = detector_.get();

    setBusy(+1);
    auto* watcher = new QFutureWatcher<Job>(this);
    connect(watcher, &QFutureWatcher<Job>::finished, this, [this, watcher] {
        jobFinished(watcher->result());
        watcher->deleteLater();
        setBusy(-1);
    });
    watcher->setFuture(QtConcurrent::run([=]() -> Job {
        Scan s = scanDocument(*det, in);
        QImage thumb = matToQImage(s.image).scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return Job{row, gen, std::move(s), std::move(thumb)};
    }));
}

void MainWindow::updateSizeRow() {
    if (current_ < 0) { heightLabel_->clear(); return; }
    const Page& p = model_->page(current_);
    QString caption;
    if (p.result.empty()) {
        caption = tr("Processing…");
    } else if (p.widthMm > 0) {
        const int w = qRound(p.widthMm), h = qRound(p.widthMm * p.result.rows / p.result.cols);
        switch (Processed::SizeSource(p.sizeSource)) {
        case Processed::SizeSource::Manual:   caption = tr("%1 × %2 mm (set manually)").arg(w).arg(h); break;
        case Processed::SizeSource::Verified: caption = tr("%1 — %2 × %3 mm, verified by %4").arg(p.standard).arg(w).arg(h).arg(p.measuredBy); break;
        case Processed::SizeSource::Assumed:  caption = tr("Shape matches %1 — %2 × %3 mm assumed").arg(p.standard).arg(w).arg(h); break;
        default:                              caption = tr("≈ %1 × %2 mm, measured by %3").arg(w).arg(h).arg(p.measuredBy); break;
        }
    } else {
        caption = tr("Size unknown — %1 × %2 px").arg(p.result.cols).arg(p.result.rows);
    }
    for (QWidget* w : {static_cast<QWidget*>(sizeBox_), static_cast<QWidget*>(widthSpin_), static_cast<QWidget*>(heightLabel_)})
        w->setToolTip(caption);

    sizeBox_->blockSignals(true);
    widthSpin_->blockSignals(true);
    int idx = p.manualName.isEmpty() ? 0 : sizeBox_->findText(p.manualName);
    if (idx < 0) idx = sizeBox_->count() - 1;   // Custom
    sizeBox_->setCurrentIndex(idx);
    const bool custom = idx == sizeBox_->count() - 1;
    widthSpin_->setReadOnly(!custom);
    widthSpin_->setButtonSymbols(custom ? QAbstractSpinBox::UpDownArrows : QAbstractSpinBox::NoButtons);
    if (p.widthMm > 0) widthSpin_->setValue(p.widthMm);
    heightLabel_->setText(p.widthMm > 0 && !p.result.empty()
        ? tr("× %1 mm").arg(qRound(p.widthMm * p.result.rows / p.result.cols)) : QString());
    sizeBox_->blockSignals(false);
    widthSpin_->blockSignals(false);
}

// Width of a named standard for the given result orientation; 0 if the name is not a standard.
static double standardWidthFor(const QString& name, bool landscape) {
    struct Std { const char* name; double w, h; };
    static const Std stds[] = {{"A4", 210, 297}, {"A5", 148, 210}, {"A3", 297, 420},
                               {"Letter", 215.9, 279.4}, {"Legal", 215.9, 355.6}, {"ID card", 85.6, 53.98}, {"DL", 220, 110}};
    for (const auto& s : stds)
        if (name == QString::fromLatin1(s.name)) {
            const double lon = std::max(s.w, s.h), sho = std::min(s.w, s.h);
            return landscape ? lon : sho;
        }
    return 0;
}

void MainWindow::applySizeOverride(Page& p) {
    // A named standard follows the result orientation (rotation may have changed it).
    if (!p.manualName.isEmpty() && !p.result.empty())
        if (const double w = standardWidthFor(p.manualName, p.result.cols > p.result.rows); w > 0)
            p.manualWidthMm = w;
    if (p.manualWidthMm > 0) {
        p.widthMm = p.manualWidthMm;
        p.sizeSource = int(Processed::SizeSource::Manual);
    } else {
        p.widthMm = p.autoWidthMm;
        p.sizeSource = p.autoSizeSource;
    }
}

void MainWindow::onSizeChoiceChanged(int idx) {
    if (current_ < 0) return;
    Page& p = model_->page(current_);
    const QString name = sizeBox_->itemText(idx);
    const bool landscape = !p.result.empty() && p.result.cols > p.result.rows;
    if (idx == 0) {                              // Auto
        p.manualWidthMm = 0;
        p.manualName.clear();
    } else if (idx == sizeBox_->count() - 1) {   // Custom: start from the current value
        p.manualWidthMm = p.widthMm > 0 ? p.widthMm : widthSpin_->value();
        p.manualName = name;
    } else if (const double w = standardWidthFor(name, landscape); w > 0) {
        p.manualWidthMm = w;
        p.manualName = name;
    }
    applySizeOverride(p);
    model_->pageChanged(current_);
    updateSizeRow();
}

void MainWindow::onManualWidthChanged(double mm) {
    if (current_ < 0) return;
    Page& p = model_->page(current_);
    p.manualWidthMm = mm;
    p.manualName = sizeBox_->itemText(sizeBox_->count() - 1);
    applySizeOverride(p);
    model_->pageChanged(current_);
    heightLabel_->setText(p.result.empty() ? QString() : tr("× %1 mm").arg(qRound(p.widthMm * p.result.rows / p.result.cols)));
}

void MainWindow::swapReference() {
    if (current_ < 0) return;
    Page& p = model_->page(current_);
    if (!p.refQuad) return;
    if (!p.swappable) return;
    std::swap(p.quad, *p.refQuad);   // both are ID-1 shaped, so the old document is a valid reference
    p.detected = true;
    p.autoRotation = -1;
    editor_->setQuad(p.quad);
    editor_->setReferenceQuad(p.refQuad);
    reprocess(current_);
}

void MainWindow::onQuadEdited(const Quad& q) {
    if (current_ < 0) return;
    Page& p = model_->page(current_);
    p.quad = q;
    p.detected = true;
    reprocess(current_);
}

void MainWindow::onModeChanged(int idx) {
    updateStrengthTip();
    if (current_ < 0) return;
    model_->page(current_).mode = ColorMode(idx);
    reprocess(current_);
}

void MainWindow::onStrengthChanged(int value) {
    strengthLabel_->setText(tr("  %1: %2%  ").arg(modeBox_->currentIndex() == 2 ? tr("Cleanliness") : tr("Enhance")).arg(value));
    if (current_ < 0) return;
    model_->page(current_).strength = value;
    strengthTimer_->start();   // debounce while dragging
}

void MainWindow::updateStrengthTip() {
    const bool bw = modeBox_->currentIndex() == 2;
    strengthLabel_->setText(tr("  %1: %2%  ").arg(bw ? tr("Cleanliness") : tr("Enhance")).arg(strength_->value()));
    strength_->setToolTip(bw
        ? tr("0 = keep faint ink (and noise), 100 = only strong ink on clean white")
        : tr("0 = photo as is, 100 = full shadow removal, white balance and sharpening"));
}

void MainWindow::redetect() {
    if (current_ < 0) return;
    Page& p = model_->page(current_);
    p.detected = false;
    p.autoRotation = -1;
    status_->setText(tr("Detecting…"));
    reprocess(current_);
}

void MainWindow::rotateResult() {
    if (current_ < 0) return;
    Page& p = model_->page(current_);
    p.rotation = (p.rotation + 90) % 360;
    reprocess(current_);
}

void MainWindow::removePage() {
    if (current_ < 0) return;
    const int row = current_;
    model_->removeRows(row, 1);
    gen_.erase(gen_.begin() + row);
    current_ = -1;
    if (model_->rowCount() > 0) list_->setCurrentIndex(model_->index(std::min(row, model_->rowCount() - 1)));
    else currentChanged(-1);
}

void MainWindow::exportPdf(bool currentOnly) {
    if (model_->rowCount() == 0 || (currentOnly && current_ < 0)) return;
    QString path = QFileDialog::getSaveFileName(this, tr("Export PDF"), "scan.pdf", tr("PDF (*.pdf)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".pdf", Qt::CaseInsensitive)) path += ".pdf";
    QString err;
    if (!exportPdfTo(path, &err, currentOnly ? current_ : -1))
        QMessageBox::warning(this, tr("Export PDF"), err);
    else
        status_->setText(tr("Saved %1").arg(path));
}

void MainWindow::exportImage(const QString& format) {
    if (current_ < 0) return;
    const Page& p = model_->page(current_);
    if (p.result.empty()) return;
    const QString upper = format.toUpper();
    QString path = QFileDialog::getSaveFileName(this, tr("Export %1").arg(upper),
        QFileInfo(p.sourcePath).completeBaseName() + "." + format, tr("%1 (*.%2)").arg(upper, format));
    if (path.isEmpty()) return;
    if (!path.endsWith("." + format, Qt::CaseInsensitive)) path += "." + format;
    if (!saveImage(path, format, p.result, p.mode, p.widthMm))
        QMessageBox::warning(this, tr("Export"), tr("Cannot write %1").arg(path));
    else
        status_->setText(tr("Saved %1").arg(path));
}

bool MainWindow::exportPdfTo(const QString& path, QString* error, int onlyRow) {
    std::vector<PdfPage> pages;
    for (int i = 0; i < model_->rowCount(); ++i) {
        if (onlyRow >= 0 && i != onlyRow) continue;
        const Page& p = model_->page(i);
        if (!p.result.empty()) pages.push_back(pdfPage(p.result, p.mode, p.widthMm));
    }
    return deltos::exportPdf(path, pages, opt_.pdf, error);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    QStringList files;
    for (const QUrl& u : e->mimeData()->urls())
        if (u.isLocalFile()) files << u.toLocalFile();
    addFiles(files);
}

} // namespace deltos
