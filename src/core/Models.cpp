#include "Models.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace deltos {

std::string findModel() {
    const QString exe = QCoreApplication::applicationDirPath();
    const QStringList dirs = {
        exe + "/models",
        exe + "/../models",
        exe + "/../Resources/models",
        exe + "/../share/deltos/models",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models",
        QDir::currentPath() + "/models",
    };
    for (const QString& d : dirs) {
        const QString f = d + "/" + kModelFile;
        if (QFileInfo::exists(f)) return QDir::cleanPath(QFileInfo(f).absoluteFilePath()).toStdString();
    }
    return {};
}

} // namespace deltos
