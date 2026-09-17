#pragma once
#include "core/Enhancer.h"
#include "export/PdfExporter.h"
#include <QImage>
#include <QString>
#include <opencv2/core.hpp>

namespace deltos {

// Processed result as a QImage ready for export: black & white becomes 1-bit.
QImage exportImage(const cv::Mat& result, ColorMode mode);

inline PdfPage pdfPage(const cv::Mat& result, ColorMode mode, double widthMm) {
    return {exportImage(result, mode), widthMm};
}

// Writes PNG or JPG ("png"/"jpg"), embedding the physical size as DPI so other
// tools show it at scale. Returns false if the file cannot be written.
bool saveImage(const QString& path, const QString& format, const cv::Mat& result, ColorMode mode, double widthMm);

} // namespace deltos
