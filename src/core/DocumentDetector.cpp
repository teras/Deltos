#include "DocumentDetector.h"
#include "CvCompat.h"
#include <filesystem>
#include <iostream>

namespace deltos {

static constexpr int kModelSize = 256;

DocumentDetector::DocumentDetector(const std::string& modelPath) {
    if (modelPath.empty() || !std::filesystem::exists(modelPath)) return;
    try {
        net_ = cv::dnn::readNetFromONNX(modelPath);
        modelName_ = std::filesystem::path(modelPath).stem().string();
    } catch (const cv::Exception& e) {
        std::cerr << "DocumentDetector: cannot load " << modelPath << ": " << e.what() << "\n";
    }
}

bool DocumentDetector::plausible(const Quad& q, const cv::Size& s) const {
    if (!q.isConvex()) return false;
    const double imgArea = double(s.width) * s.height;
    if (q.area() < 0.01 * imgArea) return false;
    for (const auto& p : q.pts)
        if (p.x < -0.05 * s.width || p.y < -0.05 * s.height ||
            p.x > 1.05 * s.width || p.y > 1.05 * s.height)
            return false;
    return true;
}

DocumentDetector::Result DocumentDetector::detect(const cv::Mat& bgr) {
    std::lock_guard lock(mutex_);
    std::vector<Candidate> cands;
    if (auto c = runModel(bgr)) cands.push_back(*c);

    // Coarse-to-fine: a document that is small in the frame is only a few pixels
    // at model resolution. Re-run the model on a crop around the coarse quad.
    if (!cands.empty()) {
        const Candidate& coarse = cands.front();
        cv::Rect box = cv::boundingRect(std::vector<cv::Point2f>(coarse.quad.pts.begin(), coarse.quad.pts.end()));
        if (box.area() < 0.5 * bgr.cols * bgr.rows) {
            const int mx = int(box.width * 0.3), my = int(box.height * 0.3);
            box = (box + cv::Point(-mx, -my) + cv::Size(2 * mx, 2 * my)) & cv::Rect(0, 0, bgr.cols, bgr.rows);
            if (box.area() > 0) {
                const cv::Mat crop = bgr(box);
                if (auto c = runModel(crop)) {
                    for (auto& p : c->quad.pts) p += cv::Point2f(float(box.x), float(box.y));
                    c->method += "/crop";
                    cands.push_back(*c);
                }
            }
        }
    }

    const auto edges = detectWithContours(bgr);
    const double tol = 0.05 * std::max(bgr.cols, bgr.rows);

    // Model output is robust but coarse; edges are precise but easily fooled.
    // If the edge quad agrees with a model candidate on at least 3 corners, the
    // edge quad (a consistent convex shape) is almost certainly the exact one:
    // take its corners and give the candidate a bonus.
    for (auto& c : cands) {
        if (!edges) break;
        int agreeing = 0;
        for (int i = 0; i < 4; ++i) agreeing += cv::norm(c.quad[i] - (*edges)[i]) < tol;
        if (agreeing >= 3) {
            c.quad = *edges;
            c.confidence = std::min(1.0, c.confidence + 0.5);
            c.method += "+edges";
        }
    }

    if (!cands.empty()) {
        auto best = std::max_element(cands.begin(), cands.end(),
            [](auto& a, auto& b) { return a.confidence < b.confidence; });
        return {best->quad, best->method, best->confidence};
    }
    if (edges) return {*edges, "edges", 0.3};
    return {Quad::fromImage(bgr.size()), "none", 0};
}

std::optional<DocumentDetector::Candidate> DocumentDetector::runModel(const cv::Mat& bgr) {
    if (bgr.empty() || net_.empty()) return std::nullopt;

    // Downscale with area averaging first: blobFromImage's bilinear resize aliases
    // badly on text from multi-megapixel photos and confuses the model.
    cv::Mat small;
    cv::resize(bgr, small, cv::Size(kModelSize, kModelSize), 0, 0, cv::INTER_AREA);
    cv::Mat blob = cv::dnn::blobFromImage(small, 1.0 / 255.0, cv::Size(kModelSize, kModelSize),
                                          cv::Scalar(), modelWantsRgb, false, CV_32F);
    net_.setInput(blob, "img");
    cv::Mat out = net_.forward("heatmap"); // 1 x 4 x H x W
    if (out.dims != 4 || out.size[1] < 4) return std::nullopt;

    const int H = out.size[2], W = out.size[3];
    std::array<cv::Point2f, 4> pts;
    double conf = 0;
    for (int c = 0; c < 4; ++c) {
        cv::Mat hm(H, W, CV_32F, out.ptr<float>(0, c));
        double peak;
        cv::minMaxLoc(hm, nullptr, &peak);
        conf += peak / 4.0;

        cv::Mat big;
        cv::resize(hm, big, bgr.size(), 0, 0, cv::INTER_LINEAR);
        // Threshold relative to the channel peak so weak but well-localised
        // responses (small documents) still yield a corner; the absolute peak
        // feeds the confidence instead.
        cv::Mat mask;
        cv::threshold(big, mask, std::max(heatmapThreshold * peak, 1e-4), 255, cv::THRESH_BINARY);
        mask.convertTo(mask, CV_8U);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (contours.empty()) return std::nullopt;
        auto best = std::max_element(contours.begin(), contours.end(),
            [](auto& a, auto& b) { return cv::contourArea(a) < cv::contourArea(b); });
        cv::Moments mo = cv::moments(*best);
        if (mo.m00 <= 0) return std::nullopt;
        pts[c] = cv::Point2f(float(mo.m10 / mo.m00), float(mo.m01 / mo.m00));
    }
    Quad q = Quad::ordered(pts);
    if (!plausible(q, bgr.size())) return std::nullopt;
    return Candidate{q, conf, modelName_};
}

std::optional<Quad> DocumentDetector::detectWithContours(const cv::Mat& bgr) {
    if (bgr.empty()) return std::nullopt;

    // Work on a downscaled copy for speed and noise robustness.
    const double scale = std::min(1.0, 800.0 / std::max(bgr.cols, bgr.rows));
    cv::Mat small;
    cv::resize(bgr, small, cv::Size(), scale, scale, cv::INTER_AREA);
    const double inv = double(bgr.cols) / small.cols;

    cv::Mat gray, edges;
    cv::cvtColor(small, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
    cv::Canny(gray, edges, 50, 150);
    cv::morphologyEx(edges, edges, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    std::sort(contours.begin(), contours.end(),
              [](auto& a, auto& b) { return cv::contourArea(a) > cv::contourArea(b); });

    const double minArea = 0.05 * small.cols * small.rows;
    for (const auto& c : contours) {
        if (cv::contourArea(c) < minArea) break;
        std::vector<cv::Point> approx;
        cv::approxPolyDP(c, approx, 0.02 * cv::arcLength(c, true), true);
        if (approx.size() != 4 || !cv::isContourConvex(approx)) continue;
        std::array<cv::Point2f, 4> pts;
        for (int i = 0; i < 4; ++i)
            pts[i] = cv::Point2f(float(approx[i].x * inv), float(approx[i].y * inv));
        Quad q = Quad::ordered(pts);
        if (plausible(q, bgr.size())) return q;
    }
    return std::nullopt;
}

} // namespace deltos
