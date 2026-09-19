#include "LanguageDialog.h"
#include "core/Tessdata.h"
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace deltos {
namespace {

// tessdata_best holds the float LSTM models, which is what we recognise with
// (OEM_LSTM_ONLY); the plain tessdata files are half legacy engine we never
// touch, and tessdata_fast is the same model quantised. Sizes are in KiB, taken
// from the repository, so the dialog can say what a download costs without
// asking the network first.
struct Entry { const char* code; int kib; };
const Entry kCatalogue[] = {
    {"afr", 12501}, {"amh", 8193}, {"ara", 12308}, {"asm", 11050}, {"aze", 6134},
    {"aze_cyrl", 4590}, {"bel", 10616}, {"ben", 10787}, {"bod", 8422}, {"bos", 5141},
    {"bre", 15274}, {"bul", 8637}, {"cat", 3713}, {"ceb", 3372}, {"ces", 10663},
    {"chi_sim", 12771}, {"chi_sim_vert", 12771}, {"chi_tra", 12681}, {"chi_tra_vert", 12681},
    {"chr", 2206}, {"cos", 8623}, {"cym", 8546}, {"dan", 9529}, {"deu", 8426},
    {"deu_latf", 12635}, {"div", 4467}, {"dzo", 3168}, {"ell", 8735}, {"eng", 15040},
    {"enm", 12970}, {"epo", 7229}, {"equ", 2199}, {"est", 15463}, {"eus", 7748}, {"fao", 9795},
    {"fas", 3248}, {"fil", 8768}, {"fin", 14033}, {"fra", 3880}, {"frm", 3948}, {"fry", 8245},
    {"gla", 9374}, {"gle", 3850}, {"glg", 12412}, {"grc", 5047}, {"guj", 8316}, {"hat", 11844},
    {"heb", 3617}, {"hin", 11617}, {"hrv", 10933}, {"hun", 12061}, {"hye", 6223}, {"iku", 5996},
    {"ind", 8060}, {"isl", 9264}, {"ita", 8656}, {"ita_old", 9621}, {"jav", 8448},
    {"jpn", 13994}, {"jpn_vert", 13995}, {"kan", 9994}, {"kat", 4382}, {"kat_old", 3100},
    {"kaz", 7352}, {"khm", 7914}, {"kir", 11668}, {"kmr", 9957}, {"kor", 12234},
    {"kor_vert", 3872}, {"lao", 13215}, {"lat", 9478}, {"lav", 5492}, {"lit", 10012},
    {"ltz", 12424}, {"mal", 12231}, {"mar", 13123}, {"mkd", 3372}, {"mlt", 4941}, {"mon", 8444},
    {"mri", 3526}, {"msa", 8038}, {"mya", 14620}, {"nep", 12097}, {"nld", 8695}, {"nor", 13977},
    {"oci", 12615}, {"ori", 7921}, {"osd", 10315}, {"pan", 11614}, {"pol", 11698},
    {"por", 7969}, {"pus", 11707}, {"que", 10522}, {"ron", 9371}, {"rus", 14943},
    {"san", 14781}, {"sin", 8089}, {"slk", 11272}, {"slv", 5741}, {"snd", 11701},
    {"spa", 13252}, {"spa_old", 9255}, {"sqi", 4523}, {"srp", 9127}, {"srp_latn", 9601},
    {"sun", 4036}, {"swa", 4800}, {"swe", 13990}, {"syr", 12205}, {"tam", 5882}, {"tat", 7407},
    {"tel", 8886}, {"tgk", 4495}, {"tha", 7436}, {"tir", 2354}, {"ton", 3642}, {"tur", 7282},
    {"uig", 12768}, {"ukr", 10605}, {"urd", 7807}, {"uzb", 12650}, {"uzb_cyrl", 4224},
    {"vie", 12144}, {"yid", 3202}, {"yor", 3649},
};

const char* kBase = "https://github.com/tesseract-ocr/tessdata_best/raw/main/";
const char* kSuffix = ".traineddata";

// Tesseract names its files by ISO 639-2 code; show what those codes mean, and
// fall back to the bare code for the ones Qt does not know (chi_sim, equ, ...).
QString languageName(const QString& code) {
    QLocale::Language lang = QLocale::AnyLanguage;
    for (auto type : {QLocale::ISO639Part2T, QLocale::ISO639Part2B, QLocale::ISO639Part3})
        if ((lang = QLocale::codeToLanguage(code, type)) != QLocale::AnyLanguage) break;
    return lang == QLocale::AnyLanguage ? code : QLocale::languageToString(lang);
}

QString sizeText(int kib) {
    return kib < 1024 ? QObject::tr("%1 KB").arg(kib)
                      : QObject::tr("%1 MB").arg(kib / 1024.0, 0, 'f', 1);
}

} // namespace

LanguageDialog::LanguageDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Recognition languages"));
    resize(520, 480);

    list_ = new QTreeWidget(this);
    list_->setColumnCount(3);
    list_->setHeaderLabels({tr("Language"), tr("Code"), tr("Size")});
    list_->setRootIsDecorated(false);
    list_->setUniformRowHeights(true);
    list_->header()->setStretchLastSection(false);
    list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    get_ = new QPushButton(tr("Download"), this);
    remove_ = new QPushButton(tr("Remove"), this);
    progress_ = new QProgressBar(this);
    progress_->setVisible(false);
    note_ = new QLabel(tr("Languages installed on the system are listed too, and are removed "
                          "the way they were installed."), this);
    note_->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto* row = new QHBoxLayout;
    row->addWidget(get_);
    row->addWidget(remove_);
    row->addWidget(progress_, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(list_, 1);
    layout->addLayout(row);
    layout->addWidget(note_);
    layout->addWidget(buttons);

    net_ = new QNetworkAccessManager(this);
    connect(list_, &QTreeWidget::currentItemChanged, this, [this] { updateButtons(); });
    connect(get_, &QPushButton::clicked, this, &LanguageDialog::download);
    connect(remove_, &QPushButton::clicked, this, &LanguageDialog::remove);
    fill();
}

// Three states per language, and the file tells them apart: a plain file in the
// user directory was downloaded here, anything in the directory Tesseract finds
// by itself came with the machine, the rest is not installed.
void LanguageDialog::fill() {
    const QString current = list_->currentItem() ? list_->currentItem()->text(1) : QString();
    list_->clear();

    QSet<QString> system;
    const QString from = Tessdata::defaultDir();
    if (!from.isEmpty())
        for (const QFileInfo& fi : QDir(from).entryInfoList({QString("*") + kSuffix}, QDir::Files))
            system << fi.completeBaseName();

    for (const Entry& e : kCatalogue) {
        const QString code = QString::fromLatin1(e.code);
        const QFileInfo own(Tessdata::fileFor(code));
        const bool mine = own.exists() && !own.isSymLink();
        const bool onSystem = system.contains(code);
        auto* item = new QTreeWidgetItem(list_);
        item->setText(0, languageName(code));
        item->setText(1, code);
        item->setText(2, mine       ? tr("Downloaded")
                        : onSystem  ? tr("On the system")
                                    : sizeText(e.kib));
        item->setData(0, Qt::UserRole, mine);
        item->setData(1, Qt::UserRole, onSystem);
        if (code == current) list_->setCurrentItem(item);
    }
    list_->sortItems(0, Qt::AscendingOrder);
    list_->resizeColumnToContents(1);
    updateButtons();
}

void LanguageDialog::updateButtons() {
    const QTreeWidgetItem* item = list_->currentItem();
    const bool busy = reply_ != nullptr;
    const bool mine = item && item->data(0, Qt::UserRole).toBool();
    const bool onSystem = item && item->data(1, Qt::UserRole).toBool();
    get_->setEnabled(item && !busy && !mine && !onSystem);
    remove_->setEnabled(item && !busy && mine);
}

void LanguageDialog::download() {
    const QTreeWidgetItem* item = list_->currentItem();
    if (!item || reply_) return;
    downloading_ = item->text(1);
    QDir().mkpath(Tessdata::userDir());

    QNetworkRequest request(QUrl(QString::fromLatin1(kBase) + downloading_ + kSuffix));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    reply_ = net_->get(request);
    progress_->setValue(0);
    progress_->setVisible(true);
    connect(reply_, &QNetworkReply::downloadProgress, this, &LanguageDialog::downloadProgress);
    connect(reply_, &QNetworkReply::finished, this, &LanguageDialog::downloadFinished);
    updateButtons();
}

void LanguageDialog::downloadProgress(qint64 got, qint64 total) {
    progress_->setMaximum(int(total / 1024));
    progress_->setValue(int(got / 1024));
}

void LanguageDialog::downloadFinished() {
    QNetworkReply* reply = reply_;
    reply_ = nullptr;
    progress_->setVisible(false);
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        note_->setText(tr("Could not download %1: %2").arg(downloading_, reply->errorString()));
        updateButtons();
        return;
    }
    // Write under a temporary name first: a half-written file would look
    // installed and then fail to load.
    const QString target = Tessdata::fileFor(downloading_);
    QFile file(target + ".part");
    if (!file.open(QIODevice::WriteOnly) || file.write(reply->readAll()) < 0) {
        note_->setText(tr("Could not save %1").arg(target));
        updateButtons();
        return;
    }
    file.close();
    QFile::remove(target);
    file.rename(target);
    fill();
    emit languagesChanged();
}

void LanguageDialog::remove() {
    const QTreeWidgetItem* item = list_->currentItem();
    if (!item) return;
    QFile::remove(Tessdata::fileFor(item->text(1)));
    fill();
    emit languagesChanged();
}

} // namespace deltos
