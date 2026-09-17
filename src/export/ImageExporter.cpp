#include "ImageExporter.h"
#include "core/CvQt.h"
#include <cmath>

namespace deltos {

QImage exportImage(const cv::Mat& result, ColorMode mode) {
    QImage img = matToQImage(result);
    if (mode == ColorMode::BlackWhite) img = img.convertToFormat(QImage::Format_Mono, Qt::ThresholdDither);
    return img;
}

bool saveImage(const QString& path, const QString& format, const cv::Mat& result, ColorMode mode, double widthMm) {
    // JPEG has no 1-bit mode; keep the grayscale there.
    QImage img = format == "png" ? exportImage(result, mode) : matToQImage(result);
    if (widthMm > 0) {
        const int dpm = int(std::lround(img.width() / (widthMm / 1000.0)));
        img.setDotsPerMeterX(dpm);
        img.setDotsPerMeterY(dpm);
    }
    return img.save(path, format.toLatin1().constData(), format == "jpg" ? 92 : -1);
}

} // namespace deltos
