#pragma once
#include <QDialog>

class QNetworkAccessManager;
class QNetworkReply;
class QTreeWidget;
class QPushButton;
class QProgressBar;
class QLabel;

namespace deltos {

// Add and remove recognition languages. Tesseract data is just a file per
// language, so there is nothing to install: downloads land in Tessdata::userDir
// and deleting the file is the uninstall. What the system already provides is
// listed too, but cannot be removed from here.
class LanguageDialog : public QDialog {
    Q_OBJECT
public:
    explicit LanguageDialog(QWidget* parent = nullptr);

signals:
    void languagesChanged();   // something was downloaded or removed

private:
    void fill();
    void updateButtons();
    void download();
    void remove();
    void downloadProgress(qint64 got, qint64 total);
    void downloadFinished();

    QTreeWidget* list_ = nullptr;
    QPushButton* get_ = nullptr;
    QPushButton* remove_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QLabel* note_ = nullptr;
    QNetworkAccessManager* net_ = nullptr;
    QNetworkReply* reply_ = nullptr;
    QString downloading_;   // language code of the transfer in flight
};

} // namespace deltos
