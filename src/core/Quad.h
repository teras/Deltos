#pragma once
#include <opencv2/core.hpp>
#include "CvCompat.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace deltos {

// Four corners of a document in image coordinates, ordered TL, TR, BR, BL.
struct Quad {
    std::array<cv::Point2f, 4> pts{};

    cv::Point2f& operator[](int i) { return pts[i]; }
    const cv::Point2f& operator[](int i) const { return pts[i]; }

    static Quad fromImage(const cv::Size& s) {
        Quad q;
        q.pts = {cv::Point2f(0, 0), cv::Point2f(float(s.width), 0),
                 cv::Point2f(float(s.width), float(s.height)), cv::Point2f(0, float(s.height))};
        return q;
    }

    // Reorder arbitrary 4 points into TL, TR, BR, BL: clockwise around the centroid
    // (well defined for any convex quad, unlike min/max of x+y which breaks near 45°),
    // starting from the corner nearest the top-left.
    static Quad ordered(const std::array<cv::Point2f, 4>& in) {
        cv::Point2f c(0, 0);
        for (const auto& p : in) c += p * 0.25f;
        Quad q;
        q.pts = in;
        std::sort(q.pts.begin(), q.pts.end(), [&](const cv::Point2f& a, const cv::Point2f& b) {
            return std::atan2(a.y - c.y, a.x - c.x) < std::atan2(b.y - c.y, b.x - c.x);
        });
        auto tl = std::min_element(q.pts.begin(), q.pts.end(),
            [](const cv::Point2f& a, const cv::Point2f& b) { return a.x + a.y < b.x + b.y; });
        std::rotate(q.pts.begin(), tl, q.pts.end());
        return q;
    }

    double area() const {
        std::vector<cv::Point2f> v(pts.begin(), pts.end());
        return std::abs(cv::contourArea(v));
    }

    bool isConvex() const {
        std::vector<cv::Point2f> v(pts.begin(), pts.end());
        return cv::isContourConvex(v);
    }
};

} // namespace deltos
