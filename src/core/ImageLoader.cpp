#include "ImageLoader.h"
#include "CvQt.h"
#include "Exif.h"
#include <QImageReader>
#include <QString>

namespace deltos {

LoadedImage loadImage(const std::string& path) {
    LoadedImage out;
    QImageReader reader(QString::fromStdString(path));
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        out.error = reader.errorString().toStdString();
        return out;
    }
    out.bgr = qImageToMat(img);
    auto ex = Exif::read(path);
    out.hasExif = ex.has_value();
    out.focalLength35mm = ex ? ex->focalLength35mm : 0;
    out.subjectDistanceMm = ex ? ex->subjectDistanceMm : 0;
    out.focalPx = Exif::focalPx(ex, out.bgr.cols, out.bgr.rows);
    return out;
}

} // namespace deltos
