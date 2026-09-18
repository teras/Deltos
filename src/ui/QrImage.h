#pragma once
#include <QImage>
#include <QString>

namespace deltos {

// Black-on-white QR code of `text`, `scale` pixels per module with the standard quiet zone.
QImage qrImage(const QString& text, int scale = 8);

} // namespace deltos
