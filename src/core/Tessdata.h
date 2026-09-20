#pragma once
#include <QString>
#include <string>

namespace deltos::Tessdata {

// Where languages the user asks for are downloaded to. Created on demand.
QString userDir();

// The directory Tesseract resolves on its own: the system packages on Linux and
// Homebrew. Empty when it cannot init at all, which means the machine has none.
QString defaultDir();

// The languages shipped with the application, looked for beside the executable.
// This is all there is inside an AppImage, an app bundle or a Windows install,
// where no system Tesseract exists to be asked.
QString bundledDir();

// The one datapath to hand to Tesseract, since it only takes one. Empty while
// nothing has been downloaded -- then Tesseract's own lookup is left alone,
// which is what already works on a machine with the distribution packages. Once
// there are downloads the two have to be merged, so everything the default
// directory holds is linked into the user one and that is used instead.
std::string dataDir();

// Where a downloaded language file belongs, whether or not it exists yet.
QString fileFor(const QString& language);

} // namespace deltos::Tessdata
