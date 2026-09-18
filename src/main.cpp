#include "cli/Cli.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QIcon>
#include <QTimer>
#include <cstring>

int main(int argc, char** argv) {
    // findModel() needs an application object for applicationDirPath(); a GUI one is
    // fine for the options themselves, so decide the mode first from argv alone.
    // Lower-case, no organisation: data in ~/.local/share/deltos (and the platform equivalents).
    QCoreApplication::setApplicationName("deltos");
    QGuiApplication::setApplicationDisplayName("Deltos");
    bool noGui = false;
    for (int i = 1; i < argc; ++i) noGui |= std::strcmp(argv[i], "--no-gui") == 0;
    if (noGui) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QGuiApplication app(argc, argv);
        const auto opt = deltos::parseOptions(argc, argv);
        return opt ? deltos::runCli(*opt) : 2;
    }

    QApplication app(argc, argv);
    const auto opt = deltos::parseOptions(argc, argv);
    if (!opt) return 2;
    QGuiApplication::setDesktopFileName("deltos"); // Wayland/GNOME match the window to the .desktop icon by this name
    QIcon icon;
    for (int s : {16, 32, 48, 64, 128, 256, 512}) icon.addFile(QStringLiteral(":/icons/deltos-%1.png").arg(s));
    QApplication::setWindowIcon(icon);
    deltos::MainWindow w(*opt);
    w.show();
    QStringList files;
    for (const std::string& f : opt->inputs) files << QString::fromStdString(f);
    if (!files.isEmpty()) w.addFiles(files);
    // Debug aid: DELTOS_SCREENSHOT=/path.png grabs the window after 3s and quits.
    if (const QByteArray shot = qgetenv("DELTOS_SCREENSHOT"); !shot.isEmpty()) {
        QTimer::singleShot(qEnvironmentVariableIntValue("DELTOS_SCREENSHOT_DELAY") > 0 ? qEnvironmentVariableIntValue("DELTOS_SCREENSHOT_DELAY") : qEnvironmentVariableIsSet("DELTOS_SWAP") ? 9000 : 3000, &w, [&w, shot] { w.grab().save(QString::fromLocal8Bit(shot)); qApp->quit(); });
    }
    // Debug aid: DELTOS_SWAP=1 presses "swap document/reference" after 5s.
    if (qEnvironmentVariableIsSet("DELTOS_SWAP"))
        QTimer::singleShot(5000, &w, [&w] { w.swapReference(); });
    // Debug aid: DELTOS_EXPORT=/path.pdf exports all pages after 8s and quits.
    if (const QByteArray out = qgetenv("DELTOS_EXPORT"); !out.isEmpty()) {
        QTimer::singleShot(8000, &w, [&w, out] { w.exportPdfTo(QString::fromLocal8Bit(out)); qApp->quit(); });
    }
    return app.exec();
}
