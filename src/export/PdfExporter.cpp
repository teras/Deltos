#include "PdfExporter.h"
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

namespace deltos {

static QPageLayout layoutFor(const PdfPage& p, int dpi) {
    const double widthPt = p.widthMm > 0 ? p.widthMm * 72.0 / 25.4 : p.image.width() * 72.0 / dpi;
    const QSizeF pts(widthPt, widthPt * p.image.height() / p.image.width());
    return QPageLayout(QPageSize(pts, QPageSize::Point, QString(), QPageSize::ExactMatch),
                       QPageLayout::Portrait, QMarginsF(0, 0, 0, 0), QPageLayout::Point);
}

bool exportPdf(const QString& path, const std::vector<PdfPage>& pages, const PdfOptions& opt, QString* error) {
    if (pages.empty()) { if (error) *error = "No pages"; return false; }

    QPdfWriter writer(path);
    writer.setResolution(opt.dpi);
    writer.setCreator(QStringLiteral("Deltos"));
    writer.setPageLayout(layoutFor(pages.front(), opt.dpi));

    QPainter painter;
    if (!painter.begin(&writer)) { if (error) *error = "Cannot write " + path; return false; }

    for (size_t i = 0; i < pages.size(); ++i) {
        const QImage& img = pages[i].image;
        if (i > 0) {
            // The layout must be set before newPage() to take effect for that page.
            writer.setPageLayout(layoutFor(pages[i], opt.dpi));
            writer.newPage();
        }
        const QRect page = writer.pageLayout().paintRectPixels(opt.dpi);
        const bool mono = img.format() == QImage::Format_Mono || img.format() == QImage::Format_MonoLSB;
        painter.setRenderHint(QPainter::LosslessImageRendering, mono);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(page, img);
    }
    painter.end();
    return true;
}

} // namespace deltos
