#pragma once
#include <QString>
#include <string>

namespace deltos::Tessdata {

// Where languages the user asks for are downloaded to. Created on demand.
QString userDir();

// The directory Tesseract resolves on its own: the system packages on Linux and
// Homebrew, the files shipped beside the application otherwise. Empty if it
// cannot init at all, which means nothing is installed anywhere.
QString defaultDir();

// The one datapath to hand to Tesseract, since it only takes one. Empty while
// nothing has been downloaded -- then Tesseract's own lookup is left alone,
// which is what already works on a machine with the distribution packages. Once
// there are downloads the two have to be merged, so everything the default
// directory holds is linked into the user one and that is used instead.
std::string dataDir();

// Where a downloaded language file belongs, whether or not it exists yet.
QString fileFor(const QString& language);

} // namespace deltos::Tessdata
