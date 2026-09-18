#include "QrImage.h"
#include "qrcodegen.hpp"

namespace deltos {

QImage qrImage(const QString& text, int scale) {
    const auto qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int border = 4, n = qr.getSize();
    QImage img((n + 2 * border) * scale, (n + 2 * border) * scale, QImage::Format_Grayscale8);
    img.fill(255);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (qr.getModule(x, y))
                for (int dy = 0; dy < scale; ++dy)
                    for (int dx = 0; dx < scale; ++dx)
                        img.setPixel((x + border) * scale + dx, (y + border) * scale + dy, 0);
    return img;
}

} // namespace deltos
