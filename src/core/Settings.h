#pragma once
#include <QStandardPaths>
#include <QString>

namespace deltos {

// ~/.config/deltos.conf (and the platform equivalents) rather than Qt's
// organisation/app tree. Shared so the CLI reads the same file the GUI writes.
inline QString settingsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/deltos.conf";
}

} // namespace deltos
