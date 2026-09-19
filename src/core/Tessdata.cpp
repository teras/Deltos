#include "Tessdata.h"
#include <tesseract/baseapi.h>
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

// Asking Tesseract beats guessing: distributions disagree on the path
// (/usr/share/tessdata, /usr/share/tesseract-ocr/5/tessdata, the prefix a
// Flatpak was built with), and it already knows which one it compiled in or
// read from TESSDATA_PREFIX.
QString defaultDir() {
    tesseract::TessBaseAPI api;
    for (const char* probe : {"eng", "osd", "ell"})
        if (api.Init(nullptr, probe, tesseract::OEM_TESSERACT_ONLY) == 0)
            return QDir::cleanPath(QString::fromUtf8(api.GetDatapath()));
    return {};
}

std::string dataDir() {
    const QStringList own = downloaded();
    if (own.isEmpty()) return {};   // nothing of ours to merge: leave Tesseract alone

    const QDir user(userDir());
    const QString from = defaultDir();
    // Drop links that have gone stale -- a language uninstalled from the system
    // would otherwise still be offered, and fail the moment it is picked.
    for (const QFileInfo& fi : user.entryInfoList({QString("*") + kSuffix}, QDir::Files | QDir::System))
        if (fi.isSymLink() && !QFileInfo::exists(fi.symLinkTarget())) QFile::remove(fi.absoluteFilePath());

    if (!from.isEmpty() && QDir::cleanPath(userDir()) != from)
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
    return QDir::cleanPath(userDir()).toStdString();
}

} // namespace deltos::Tessdata
