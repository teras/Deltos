#pragma once
#include "core/Quad.h"
#include "core/Enhancer.h"
#include <QAbstractListModel>
#include <QImage>
#include <QString>
#include <optional>
#include <opencv2/core.hpp>
#include <vector>

namespace deltos {

struct Page {
    QString sourcePath;
    cv::Mat source;        // BGR, EXIF-oriented
    double focalPx = 0;
    Quad quad;
    bool detected = false; // quad has been set (auto or by hand)
    QString detection;     // method name of the last automatic detection
    ColorMode mode = ColorMode::Color;
    int strength = 50;     // 0..100, meaning depends on mode (see Enhancer)
    int autoRotation = -1; // text-orientation fix from OSD, degrees CW; -1 = not yet detected
    int rotation = 0;      // extra user rotation on top, degrees CW (0/90/180/270)
    cv::Mat result;        // processed output (may be empty until computed)
    double autoWidthMm = 0;  // physical width found automatically (0 = unknown)
    int autoSizeSource = 0;  // Processed::SizeSource of the automatic value
    double widthMm = 0;      // effective width: manual override if set, else automatic
    int sizeSource = 0;      // Processed::SizeSource of the effective value
    QString measuredBy;
    double subjectDistanceMm = 0;
    std::optional<Quad> refQuad;   // reference card found beside the document (source coords)
    double refWidthMm = 0;         // document width measured through it
    bool swappable = false;        // document and reference have the same (ID-1) shape
    double manualWidthMm = 0;      // user override (width of the displayed result), 0 = auto
    QString manualName;            // "A4", "Custom", ... for display
    QString standard;      // "A4", "Letter", "ID-1" or empty
    QImage thumbnail;
};

class PageModel : public QAbstractListModel {
    Q_OBJECT
public:
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& = {}) const override { return int(pages_.size()); }
    QVariant data(const QModelIndex& idx, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& idx) const override;
    Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }
    bool moveRows(const QModelIndex&, int src, int count, const QModelIndex&, int dst) override;
    bool removeRows(int row, int count, const QModelIndex& = {}) override;

    int addPage(Page page);
    Page& page(int row) { return pages_[size_t(row)]; }
    const Page& page(int row) const { return pages_[size_t(row)]; }
    void pageChanged(int row);

private:
    std::vector<Page> pages_;
};

} // namespace deltos
