#include "Enhancer.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace deltos::Enhancer {

// Flatten uneven illumination: divide by a heavily blurred background estimate.
static cv::Mat backgroundOf(const cv::Mat& gray);
static cv::Mat removeShadows(const cv::Mat& gray) {
    cv::Mat out;
    cv::divide(gray, backgroundOf(gray), out, 255.0);
    return out;
}

static cv::Mat unsharp(const cv::Mat& img, double amount = 0.6) {
    cv::Mat blur;
    cv::GaussianBlur(img, blur, {0, 0}, 2.0);
    cv::Mat out;
    cv::addWeighted(img, 1.0 + amount, blur, -amount, 0, out);
    return out;
}

// Large-scale illumination estimate (background without ink).
static cv::Mat backgroundOf(const cv::Mat& gray) {
    cv::Mat bg;
    const int k = std::max(31, (std::max(gray.cols, gray.rows) / 20) | 1);
    cv::morphologyEx(gray, bg, cv::MORPH_CLOSE, cv::getStructuringElement(cv::MORPH_ELLIPSE, {k, k}));
    cv::GaussianBlur(bg, bg, {k, k}, 0);
    return bg;
}

static double percentile(const cv::Mat& m8u, double p) {
    int hist[256] = {};
    for (int y = 0; y < m8u.rows; ++y)
        for (int x = 0; x < m8u.cols; ++x) ++hist[m8u.at<uchar>(y, x)];
    const long total = long(m8u.rows) * m8u.cols;
    long acc = 0;
    for (int v = 0; v < 256; ++v) { acc += hist[v]; if (acc >= p * total) return v; }
    return 255;
}

// Lift shadows only: gain = paperLevel / background, clamped to [1, maxGain].
// Uniformly dark objects (a dark card) keep their colours; shaded paper is brightened.
static cv::Mat liftShadows(const cv::Mat& bgr) {
    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::Mat bg = backgroundOf(gray);
    const double paper = percentile(bg, 0.95);
    cv::Mat gain;
    bg.convertTo(gain, CV_32F);
    gain = paper / gain;
    cv::min(gain, 3.0f, gain);
    cv::max(gain, 1.0f, gain);
    cv::Mat f, out;
    bgr.convertTo(f, CV_32FC3);
    cv::Mat ch[3];
    cv::split(f, ch);
    for (auto& c : ch) cv::multiply(c, gain, c);
    cv::merge(ch, 3, f);
    f.convertTo(out, CV_8UC3);
    return out;
}

// Neutralise the paper tint, but only when there is bright paper to measure.
static cv::Mat paperWhiteBalance(const cv::Mat& bgr) {
    cv::Mat ch[3];
    cv::split(bgr, ch);
    double white[3];
    for (int i = 0; i < 3; ++i) white[i] = percentile(backgroundOf(ch[i]), 0.95);
    const double mx = std::max({white[0], white[1], white[2]});
    if (mx < 170) return bgr;  // no white paper in view (dark object): leave colours alone
    for (int i = 0; i < 3; ++i)
        if (white[i] > 1) ch[i].convertTo(ch[i], -1, std::min(1.4, mx / white[i]), 0);
    cv::Mat out;
    cv::merge(ch, 3, out);
    return out;
}

static cv::Mat blend(const cv::Mat& raw, const cv::Mat& enhanced, double s) {
    if (s >= 1.0) return enhanced;
    if (s <= 0.0) return raw;
    cv::Mat out;
    cv::addWeighted(raw, 1.0 - s, enhanced, s, 0, out);
    return out;
}

cv::Mat enhance(const cv::Mat& bgr, ColorMode mode, double strength) {
    const double s = std::clamp(strength, 0.0, 1.0);
    switch (mode) {
    case ColorMode::Color:
        if (s <= 0.0) return bgr;
        return blend(bgr, unsharp(paperWhiteBalance(liftShadows(bgr)), 0.4), s);
    case ColorMode::Gray: {
        cv::Mat gray;
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
        if (s <= 0.0) return gray;
        return blend(gray, unsharp(removeShadows(gray)), s);
    }
    case ColorMode::BlackWhite: {
        cv::Mat gray;
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
        gray = removeShadows(gray);
        cv::Mat bw;
        const int block = std::max(15, (std::max(gray.cols, gray.rows) / 40) | 1);
        // Offset subtracted from the local mean: small keeps faint strokes, large keeps only dark ink.
        const double c = 4.0 + 26.0 * s;
        cv::adaptiveThreshold(gray, bw, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, block, c);
        return bw;
    }
    }
    return bgr;
}

} // namespace deltos::Enhancer
