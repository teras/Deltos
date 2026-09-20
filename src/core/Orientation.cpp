#include "Orientation.h"
#include "Ocr.h"
#include "Tessdata.h"
#include <opencv2/imgproc.hpp>
#include <tesseract/baseapi.h>
#include <iostream>
#include <memory>
#include <cctype>
#include <vector>
#include <opencv2/core/utility.hpp>
#include <tesseract/resultiterator.h>

namespace deltos::Orientation {

Result detect(const cv::Mat& img, const std::string& tessdataDir) {
    Result r;
    if (img.empty()) return r;

    cv::Mat gray;
    if (img.channels() == 3) cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    else gray = img;
    // OSD needs enough text pixels but not a huge image.
    const double scale = 1500.0 / std::max(gray.cols, gray.rows);
    if (scale < 1.0) cv::resize(gray, gray, cv::Size(), scale, scale, cv::INTER_AREA);
    gray = gray.clone(); // ensure continuous

    const std::string dir = tessdataDir.empty() ? Tessdata::dataDir() : tessdataDir;
    const char* dataDir = dir.empty() ? nullptr : dir.c_str();

    // 1) OSD: fast and reliable on pages with plenty of text.
    {
        tesseract::TessBaseAPI api;
        if (api.Init(dataDir, "osd", tesseract::OEM_TESSERACT_ONLY) != 0) {
            std::cerr << "Orientation: cannot init tesseract OSD\n";
            return r;
        }
        api.SetPageSegMode(tesseract::PSM_OSD_ONLY);
        api.SetImage(gray.data, gray.cols, gray.rows, 1, int(gray.step));
        api.SetSourceResolution(300);
        int orientDeg = 0;
        float orientConf = 0, scriptConf = 0;
        const char* script = nullptr;
        if (api.DetectOrientationScript(&orientDeg, &orientConf, &script, &scriptConf)) {
            // orientDeg is how far the text is rotated clockwise from upright
            // (verified empirically), so undo it by rotating counter-clockwise.
            r.degreesCW = ((360 - orientDeg) % 360 + 360) % 360;
            r.confidence = orientConf;
            r.ok = true;
        }
    }
    if (r.ok) return r;

    // 2) Fallback for sparse text (cards, labels): OCR each rotation and keep the
    //    one that reads best. Score = word confidence weighted by text amount.
    // Bring the text to a size Tesseract likes and binarise against the local
    // background, once for dark ink and once for light ink (cards, labels).
    cv::Mat work;
    const double s2 = std::clamp(1600.0 / std::max(gray.cols, gray.rows), 0.5, 2.0);
    cv::resize(gray, work, cv::Size(), s2, s2, s2 > 1 ? cv::INTER_CUBIC : cv::INTER_AREA);
    cv::Mat bg, diff;
    cv::GaussianBlur(work, bg, cv::Size(0, 0), 25);
    cv::subtract(work, bg, diff, cv::noArray(), CV_16S);
    // Both rendered as black ink on white, which is what Tesseract expects.
    cv::Mat darkInk, lightInk;
    cv::threshold(diff, darkInk, -18, 255, cv::THRESH_BINARY);      // darker than surroundings -> black
    cv::threshold(diff, lightInk, 18, 255, cv::THRESH_BINARY_INV);  // brighter than surroundings -> black
    darkInk.convertTo(darkInk, CV_8U);
    lightInk.convertTo(lightInk, CV_8U);

    // Tesseract is single-threaded; the 8 (rotation x polarity) runs are independent,
    // so each gets its own API instance and they run in parallel.
    struct Task { int rot; cv::Mat img; double score = 0; };
    std::vector<Task> tasks;
    for (int rot = 0; rot < 4; ++rot)
        for (const cv::Mat& src : {darkInk, lightInk}) {
            cv::Mat img;
            if (rot == 0) img = src.clone();
            else cv::rotate(src, img, rot == 1 ? cv::ROTATE_90_CLOCKWISE : rot == 2 ? cv::ROTATE_180 : cv::ROTATE_90_COUNTERCLOCKWISE);
            tasks.push_back({rot, img});
        }

    // Resolved once: every instance below wants it, and working it out probes the
    // installed traineddata. Which language hardly matters here -- the score only
    // counts clean, confident words -- but it must not be someone's favourite two.
    const std::string language = Ocr::defaultLanguage(dir);
    if (language.empty()) return r;   // nothing installed: recognising in nothing crashes Tesseract

    cv::parallel_for_(cv::Range(0, int(tasks.size())), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; ++i) {
            tesseract::TessBaseAPI api;
            if (api.Init(dataDir, language.c_str(), tesseract::OEM_LSTM_ONLY) != 0) continue;
            api.SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
            const cv::Mat& img = tasks[size_t(i)].img;
            api.SetImage(img.data, img.cols, img.rows, 1, int(img.step));
            api.SetSourceResolution(150);
            if (api.Recognize(nullptr) != 0) continue;
            double score = 0;
            std::unique_ptr<tesseract::ResultIterator> it(api.GetIterator());
            if (!it) continue;
            do {
                const float conf = it->Confidence(tesseract::RIL_WORD);
                std::unique_ptr<char[]> txt(it->GetUTF8Text(tesseract::RIL_WORD));
                if (!txt) continue;
                // Count letters/digits as code points (not UTF-8 bytes); anything else disqualifies the word.
                int letters = 0;
                bool clean = true;
                for (const char* c = txt.get(); *c; ++c) {
                    const uchar b = uchar(*c);
                    if ((b & 0xC0) == 0x80) continue;             // UTF-8 continuation byte
                    if (b >= 0x80 || std::isalnum(b)) ++letters;  // non-ASCII (Greek etc.) or ASCII alnum
                    else if (b != '.' && b != ',' && b != '-' && b != '\'') clean = false;
                }
                if (clean && conf > 50 && letters >= 3) score += conf * letters;
            } while (it->Next(tesseract::RIL_WORD));
            tasks[size_t(i)].score = score;
        }
    });

    double rotScore[4] = {};
    for (const auto& t : tasks) rotScore[t.rot] = std::max(rotScore[t.rot], t.score);
    double bestScore = 0, secondScore = 0;
    int bestRot = 0;
    for (int rot = 0; rot < 4; ++rot) {
        if (getenv("DELTOS_ORIENT_DEBUG")) std::cerr << "  rot " << rot * 90 << " score=" << rotScore[rot] << "\n";
        if (rotScore[rot] > bestScore) { secondScore = bestScore; bestScore = rotScore[rot]; bestRot = rot; }
        else if (rotScore[rot] > secondScore) secondScore = rotScore[rot];
    }
    if (bestScore <= 0 || bestScore < 1.3 * secondScore) return r;   // no clear winner: leave as is
    r.degreesCW = bestRot * 90;
    r.confidence = secondScore > 0 ? bestScore / secondScore : bestScore;
    r.ok = true;
    return r;
}

} // namespace deltos::Orientation
