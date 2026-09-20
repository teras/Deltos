#include "ImageLoader.h"
#include "CvQt.h"
#include "Exif.h"
#include <QImageReader>
#include <QString>
#ifdef DELTOS_HAVE_HEIF
#include <libheif/heif.h>
#endif

namespace deltos {
namespace {

#ifdef DELTOS_HAVE_HEIF
// Qt reads HEIC only where the kimageformats plugin is installed, which is
// nowhere on Windows and not everywhere on Linux -- and HEIC is what an iPhone
// produces. libheif is already linked for the EXIF block, so decode with it
// when Qt comes back empty-handed. It turns the picture the right way up by
// itself, from the container's own rotation: checked against the Qt plugin on
// an iPhone photograph, both give the same 4284x5712 and the same page, where
// applying the EXIF orientation on top of it turned the image on its side.
QImage readHeif(const std::string& path) {
    QImage out;
    heif_context* ctx = heif_context_alloc();
    if (heif_context_read_from_file(ctx, path.c_str(), nullptr).code == heif_error_Ok) {
        heif_image_handle* handle = nullptr;
        if (heif_context_get_primary_image_handle(ctx, &handle).code == heif_error_Ok) {
            heif_image* img = nullptr;
            if (heif_decode_image(handle, &img, heif_colorspace_RGB,
                                  heif_chroma_interleaved_RGB, nullptr).code == heif_error_Ok) {
                int stride = 0;
                const uint8_t* p = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
                const int w = heif_image_get_width(img, heif_channel_interleaved);
                const int h = heif_image_get_height(img, heif_channel_interleaved);
                if (p && w > 0 && h > 0)
                    out = QImage(p, w, h, stride, QImage::Format_RGB888).copy();
                heif_image_release(img);
            }
            heif_image_handle_release(handle);
        }
    }
    heif_context_free(ctx);
    return out;
}

#endif

} // namespace

LoadedImage loadImage(const std::string& path) {
    LoadedImage out;
    QImageReader reader(QString::fromStdString(path));
    reader.setAutoTransform(true);
    QImage img = reader.read();
    auto ex = Exif::read(path);
#ifdef DELTOS_HAVE_HEIF
    if (img.isNull()) img = readHeif(path);
#endif
    if (img.isNull()) {
        out.error = reader.errorString().toStdString();
        return out;
    }
    out.bgr = qImageToMat(img);
    out.hasExif = ex.has_value();
    out.focalLength35mm = ex ? ex->focalLength35mm : 0;
    out.subjectDistanceMm = ex ? ex->subjectDistanceMm : 0;
    out.focalPx = Exif::focalPx(ex, out.bgr.cols, out.bgr.rows);
    return out;
}

} // namespace deltos
