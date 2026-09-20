#include "Tessdata.h"
#include <tesseract/baseapi.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace deltos::Tessdata {
namespace {

const char* kSuffix = ".traineddata";

// Files the user downloaded, as opposed to the links to the default directory
// that dataDir() leaves behind.
QStringList downloaded() {
    QStringList own;
    const QDir dir(userDir());
    for (const QFileInfo& fi : dir.entryInfoList({QString("*") + kSuffix}, QDir::Files))
        if (!fi.isSymLink()) own << fi.fileName();
    return own;
}

} // namespace

QString userDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tessdata";
}

QString fileFor(const QString& language) {
    return userDir() + "/" + language + kSuffix;
}

// Beside the executable, the way the detection model is found: a distribution
// package has none of these and falls through to what Tesseract knows, while a
// self-contained build has nothing else.
QString bundledDir() {
    const QString exe = QCoreApplication::applicationDirPath();
    for (const QString& dir : {exe + "/tessdata", exe + "/../tessdata",
                               exe + "/../share/tessdata", exe + "/../share/deltos/tessdata",
                               exe + "/../Resources/tessdata"})
        if (QDir(dir).exists() && !QDir(dir).entryList({QString("*") + kSuffix}, QDir::Files).isEmpty())
            return QDir::cleanPath(QDir(dir).absolutePath());
    return {};
}

// Asking Tesseract beats guessing: distributions disagree on the path
// (/usr/share/tessdata, /usr/share/tesseract-ocr/5/tessdata, the prefix a
// Flatpak was built with), and it already knows which one it compiled in or
// read from TESSDATA_PREFIX.
QString defaultDir() {
    tesseract::TessBaseAPI api;
    for (const char* probe : {"eng", "osd", "ell"})
        if (api.Init(nullptr, probe, tesseract::OEM_LSTM_ONLY) == 0)
            return QDir::cleanPath(QString::fromUtf8(api.GetDatapath()));
    return {};
}

std::string dataDir() {
    const QStringList own = downloaded();
    const QString bundled = bundledDir();
    const QString system = defaultDir();
    if (own.isEmpty()) {
        // Nothing of ours to merge: leave Tesseract's own lookup alone, which is
        // what already works where the distribution provides the languages.
        if (bundled.isEmpty()) return {};
        // Nothing but what we ship -- a self-contained build on a machine with
        // no Tesseract of its own -- so hand that over and skip the merging.
        if (system.isEmpty()) return bundled.toStdString();
    }

    const QDir user(userDir());
    QDir().mkpath(userDir());   // there may be nothing downloaded yet, only ours to merge
    // Drop links that have gone stale -- a language uninstalled from the system
    // would otherwise still be offered, and fail the moment it is picked.
    for (const QFileInfo& fi : user.entryInfoList({QString("*") + kSuffix}, QDir::Files | QDir::System))
        if (fi.isSymLink() && !QFileInfo::exists(fi.symLinkTarget())) QFile::remove(fi.absoluteFilePath());

    // Everything the user can already recognise in, linked into one directory,
    // since Tesseract accepts only one and the downloads have to live with the
    // rest. The system comes first: where a language exists both there and in
    // what we ship, the one the machine maintains is the one to use.
    for (const QString& from : {system, bundled}) {
        if (from.isEmpty() || QDir::cleanPath(userDir()) == from) continue;
        for (const QFileInfo& fi : QDir(from).entryInfoList({QString("*") + kSuffix}, QDir::Files)) {
            const QString link = user.filePath(fi.fileName());
            if (!QFileInfo::exists(link)) {
#ifdef Q_OS_WIN
                QFile::copy(fi.absoluteFilePath(), link);   // no usable symlinks without privileges
#else
                QFile::link(fi.absoluteFilePath(), link);
#endif
            }
        }
    }
    return QDir::cleanPath(userDir()).toStdString();
}

} // namespace deltos::Tessdata
