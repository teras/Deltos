#include "Ocr.h"
#include "Tessdata.h"
#include <opencv2/imgproc.hpp>
#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <QLocale>

namespace deltos::Ocr {
namespace {

// Tesseract hands back whole blocks one after another, so a column of labels can
// arrive before text that sits above it on the page. Selection is a range over
// this order, so put the lines back into the order the eye scans them: down the
// page, and left to right where two lines sit side by side.
void sortByLayout(std::vector<Word>& words) {
    if (words.empty()) return;

    struct Line { int top = 0, bottom = 0, left = 0; std::vector<size_t> words; };
    std::vector<Line> lines;
    std::vector<int> lineOf(size_t(words.back().line) + 1, -1);
    for (size_t i = 0; i < words.size(); ++i) {
        int& slot = lineOf[size_t(words[i].line)];
        if (slot < 0) {
            slot = int(lines.size());
            lines.push_back({words[i].box.y, words[i].box.br().y, words[i].box.x, {}});
        }
        Line& l = lines[size_t(slot)];
        l.top = std::min(l.top, words[i].box.y);
        l.bottom = std::max(l.bottom, words[i].box.br().y);
        l.left = std::min(l.left, words[i].box.x);
        l.words.push_back(i);
    }

    // Group lines into horizontal bands: two lines share a band when they overlap
    // vertically by half the shorter one, which is what "side by side" looks like.
    std::vector<size_t> order(lines.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return lines[a].top < lines[b].top; });
    std::vector<int> band(lines.size(), 0);
    size_t first = order.front();
    int current = 0;
    for (size_t k = 1; k < order.size(); ++k) {
        const Line& f = lines[first];
        const Line& l = lines[order[k]];
        const int overlap = std::min(f.bottom, l.bottom) - std::max(f.top, l.top);
        const int shorter = std::min(f.bottom - f.top, l.bottom - l.top);
        if (shorter <= 0 || overlap * 2 < shorter) { ++current; first = order[k]; }
        band[order[k]] = current;
    }
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return band[a] != band[b] ? band[a] < band[b] : lines[a].left < lines[b].left;
    });

    std::vector<Word> sorted;
    sorted.reserve(words.size());
    for (size_t k = 0; k < order.size(); ++k)
        for (size_t i : lines[order[k]].words) {
            sorted.push_back(std::move(words[i]));
            sorted.back().line = int(k);   // renumbered, so a line break is still line != line
        }
    words = std::move(sorted);
}

} // namespace

std::vector<std::string> availableLanguages(const std::string& tessdataDir) {
    const std::string dir = tessdataDir.empty() ? Tessdata::dataDir() : tessdataDir;
    const char* dataDir = dir.empty() ? nullptr : dir.c_str();
    tesseract::TessBaseAPI api;
    // Listing needs an initialised instance, and Init needs a language that
    // exists: try the ones we ship against before giving up.
    for (const char* probe : {"eng", "ell", "osd"})
        if (api.Init(dataDir, probe, tesseract::OEM_TESSERACT_ONLY) == 0) {
            std::vector<std::string> langs;
            api.GetAvailableLanguagesAsVector(&langs);
            std::sort(langs.begin(), langs.end());
            return langs;
        }
    return {};
}

// Qt speaks ISO 639-1 ("el"), Tesseract names its files in ISO 639-2 ("ell"),
// which has two variants for some languages (deu/ger, fra/fre) -- try each and
// keep whichever is actually installed.
std::string defaultLanguage(const std::string& tessdataDir) {
    const std::vector<std::string> have = availableLanguages(tessdataDir);
    const auto installed = [&](const std::string& l) {
        return std::find(have.begin(), have.end(), l) != have.end();
    };
    std::string mine;
    for (const QString& tag : QLocale::system().uiLanguages()) {
        const QLocale::Language lang = QLocale(tag).language();
        for (auto part : {QLocale::ISO639Part2T, QLocale::ISO639Part2B, QLocale::ISO639Part3}) {
            const std::string code = QLocale::languageToCode(lang, part).toStdString();
            if (!code.empty() && installed(code)) { mine = code; break; }
        }
        if (!mine.empty()) break;
    }
    // Most documents carry some Latin text (amounts, codes, URLs) whatever their
    // language, so English comes along whenever it is available.
    if (mine.empty() || mine == "eng") return installed("eng") ? "eng" : mine;
    return installed("eng") ? mine + "+eng" : mine;
}

Result run(const cv::Mat& img, const std::string& language, const std::string& tessdataDir) {
    Result r;
    if (img.empty()) return r;

    cv::Mat gray;
    if (img.channels() == 3) cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    else gray = img;
    // Tesseract reads best around 300 dpi, which for an A4 long side is ~3500 px.
    // Only shrink what is clearly above that; shrinking a normal page loses the
    // small print (amounts, dates) entirely.
    const double longSide = std::max(gray.cols, gray.rows);
    const double scale = std::clamp(3500.0 / longSide, 0.5, 2.0);
    if (std::abs(scale - 1.0) > 0.05)
        cv::resize(gray, gray, cv::Size(), scale, scale, scale > 1 ? cv::INTER_CUBIC : cv::INTER_AREA);
    gray = gray.clone();   // ensure continuous

    const double inv = gray.cols > 0 ? double(img.cols) / gray.cols : 1.0;

    tesseract::TessBaseAPI api;
    const std::string dir = tessdataDir.empty() ? Tessdata::dataDir() : tessdataDir;
    const char* dataDir = dir.empty() ? nullptr : dir.c_str();
    if (api.Init(dataDir, language.c_str(), tesseract::OEM_LSTM_ONLY) != 0) {
        std::cerr << "Ocr: cannot init tesseract for \"" << language << "\"\n";
        return r;
    }
    api.SetPageSegMode(tesseract::PSM_AUTO);
    api.SetImage(gray.data, gray.cols, gray.rows, 1, int(gray.step));
    api.SetSourceResolution(300);
    if (api.Recognize(nullptr) != 0) return r;

    std::unique_ptr<tesseract::ResultIterator> it(api.GetIterator());
    if (!it) return r;
    int line = -1;
    do {
        if (it->IsAtBeginningOf(tesseract::RIL_TEXTLINE)) ++line;
        std::unique_ptr<char[]> txt(it->GetUTF8Text(tesseract::RIL_WORD));
        if (!txt) continue;
        std::string s = txt.get();
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        const float conf = it->Confidence(tesseract::RIL_WORD);
        if (s.empty() || conf < 25) continue;   // low enough that a read word always gets a box
        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        if (!it->BoundingBox(tesseract::RIL_WORD, &x1, &y1, &x2, &y2)) continue;
        Word w;
        w.text = std::move(s);
        w.box = cv::Rect(cv::Point(int(x1 * inv), int(y1 * inv)), cv::Point(int(x2 * inv), int(y2 * inv)));
        w.confidence = conf;
        w.line = std::max(line, 0);
        r.words.push_back(std::move(w));
    } while (it->Next(tesseract::RIL_WORD));

    sortByLayout(r.words);

    // The plain text is built from the words that survived, so "copy all" and the
    // boxes on screen always show the same thing.
    for (size_t i = 0; i < r.words.size(); ++i) {
        if (i) r.text += r.words[i].line != r.words[i - 1].line ? '\n' : ' ';
        r.text += r.words[i].text;
    }
    return r;
}

} // namespace deltos::Ocr
