#pragma once
#include <QImage>
#include <QString>
#include <vector>

namespace deltos {

struct PdfPage {
    QImage image;
    double widthMm = 0;   // physical width; 0 = derive from pixels at PdfOptions::dpi
};

struct PdfOptions {
    int dpi = 300;   // fallback pixel density when the physical size is unknown
};

// One PDF page per image, each page sized to its own document.
// Mono (1-bit) images are kept lossless; the rest are JPEG-compressed by Qt.
bool exportPdf(const QString& path, const std::vector<PdfPage>& pages, const PdfOptions& opt, QString* error = nullptr);

} // namespace deltos
