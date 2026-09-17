#include "Geometry.h"
#include "CvCompat.h"
#include <cmath>
#include <cstring>

namespace deltos::Geometry {

static double edgeRatioFallback(const Quad& q) {
    const double top = cv::norm(q[1] - q[0]), bottom = cv::norm(q[2] - q[3]);
    const double left = cv::norm(q[3] - q[0]), right = cv::norm(q[2] - q[1]);
    const double w = std::max(top, bottom), h = std::max(left, right);
    return h > 0 ? w / h : 1.0;
}

double aspectRatio(const Quad& q, const cv::Size& imageSize, double focalPx) {
    // Homogeneous corners with principal point at the image centre.
    const double u0 = imageSize.width / 2.0, v0 = imageSize.height / 2.0;
    auto H = [&](const cv::Point2f& p) { return cv::Vec3d(p.x - u0, p.y - v0, 1.0); };
    // Paper ordering: m1 = TL, m2 = TR, m3 = BL, m4 = BR
    const cv::Vec3d m1 = H(q[0]), m2 = H(q[1]), m3 = H(q[3]), m4 = H(q[2]);

    const double d2 = m2.cross(m4).dot(m3);
    const double d3 = m3.cross(m4).dot(m2);
    if (std::abs(d2) < 1e-9 || std::abs(d3) < 1e-9) return edgeRatioFallback(q);

    const double k2 = m1.cross(m4).dot(m3) / d2;
    const double k3 = m1.cross(m4).dot(m2) / d3;
    const cv::Vec3d n2 = k2 * m2 - m1;
    const cv::Vec3d n3 = k3 * m3 - m1;

    const double longSide = std::max(imageSize.width, imageSize.height);
    double f = focalPx;
    if (f <= 0) {
        // Self-calibration works only when both edge pairs show perspective;
        // accept it only within a plausible camera range, else assume ~28mm equiv.
        const double denom = n2[2] * n3[2];
        if (std::abs(denom) > 1e-9) {
            const double f2 = -(n2[0] * n3[0] + n2[1] * n3[1]) / denom;
            if (f2 > 0) {
                const double fe = std::sqrt(f2);
                if (fe > 0.4 * longSide && fe < 3.0 * longSide) f = fe;
            }
        }
        if (f <= 0) f = 0.78 * longSide;
    }

    // ratio^2 = (n2^T A^-T A^-1 n2) / (n3^T A^-T A^-1 n3), A = diag(f, f, 1)
    auto norm2 = [&](const cv::Vec3d& n) {
        return (n[0] * n[0] + n[1] * n[1]) / (f * f) + n[2] * n[2];
    };
    const double num = norm2(n2), den = norm2(n3);
    if (den <= 0 || num <= 0 || !std::isfinite(num / den)) return edgeRatioFallback(q);
    const double r = std::sqrt(num / den);
    // Reject absurd results (the closed form is unstable for near-frontal shots).
    if (r < 0.2 || r > 5.0) return edgeRatioFallback(q);
    return r;
}

namespace {
struct Size { const char* name; double w, h; const char* family; bool common; };
// Common member of each family is listed first.
const Size kSizes[] = {
    {"A4", 210, 297, "A-series", true}, {"A5", 148, 210, "A-series", false},
    {"A3", 297, 420, "A-series", false}, {"A6", 105, 148, "A-series", false},
    {"Letter", 215.9, 279.4, "Letter", true},
    {"Legal", 215.9, 355.6, "Legal", true},
    {"ID-1", 85.6, 53.98, "ID-1", true},
    {"DL", 220, 110, "DL", true},
};
}

Standard snapToStandard(double ratio, double tolerance, double measuredWidthMm) {
    Standard best{ratio, 0, "", ""};
    double bestErr = tolerance;
    const Size* bestSize = nullptr;
    bool landscape = false;
    for (const auto& sz : kSizes) {
        if (!sz.common) continue;   // families are matched by ratio via their common member
        for (int rot = 0; rot < 2; ++rot) {
            const double w = rot ? sz.h : sz.w, h = rot ? sz.w : sz.h;
            const double err = std::abs(ratio - w / h) / (w / h);
            if (err < bestErr) { bestErr = err; bestSize = &sz; landscape = rot; }
        }
    }
    if (!bestSize) return best;

    // Within the family, pick the member closest to the measured width (if any).
    const Size* pick = bestSize;
    if (measuredWidthMm > 0) {
        double bestDiff = 1e9;
        for (const auto& sz : kSizes) {
            if (std::strcmp(sz.family, bestSize->family) != 0) continue;
            const double w = landscape ? sz.h : sz.w;
            const double diff = std::abs(w - measuredWidthMm) / w;
            if (diff < bestDiff) { bestDiff = diff; pick = &sz; }
        }
        // A measurement that fits no member at all means the shape match was a coincidence.
        if (bestDiff > 0.30) return best;
    }
    const double w = landscape ? pick->h : pick->w, h = landscape ? pick->w : pick->h;
    return Standard{w / h, w, pick->name, pick->family};
}

cv::Size outputSize(const Quad& q, double ratio) {
    const double top = cv::norm(q[1] - q[0]), bottom = cv::norm(q[2] - q[3]);
    const double left = cv::norm(q[3] - q[0]), right = cv::norm(q[2] - q[1]);
    // Keep the resolution of the longer visible dimension.
    const double w = std::max(top, bottom), h = std::max(left, right);
    int outW, outH;
    if (w / h >= ratio) { outW = int(std::lround(w)); outH = int(std::lround(w / ratio)); }
    else                { outH = int(std::lround(h)); outW = int(std::lround(h * ratio)); }
    return {std::max(outW, 1), std::max(outH, 1)};
}

double widthFromDistance(const Quad& q, double ratio, double distanceMm, double focalPx) {
    const cv::Size out = outputSize(q, ratio);
    const cv::Point2f dst[4] = {{0.f, 0.f}, {float(out.width), 0.f},
                                {float(out.width), float(out.height)}, {0.f, float(out.height)}};
    const cv::Mat H = cv::getPerspectiveTransform(dst, q.pts.data()); // rectified -> image
    auto map = [&](double x, double y) {
        std::vector<cv::Point2f> in{cv::Point2f(float(x), float(y))}, res;
        cv::perspectiveTransform(in, res, H);
        return res[0];
    };
    // Jacobian at the centre of the rectified page; its largest singular value is the
    // image scale (px per rectified px) of the least foreshortened in-plane direction,
    // which at distance Z equals focalPx / Z per mm.
    const double cx = out.width / 2.0, cy = out.height / 2.0;
    const cv::Point2f dx = map(cx + 1, cy) - map(cx - 1, cy);
    const cv::Point2f dy = map(cx, cy + 1) - map(cx, cy - 1);
    const cv::Matx22d J(dx.x / 2, dy.x / 2, dx.y / 2, dy.y / 2);
    cv::Matx21d sv;
    cv::SVD::compute(J, sv);
    const double pxPerRectifiedPx = sv(0);
    const double mmPerRectifiedPx = pxPerRectifiedPx * distanceMm / focalPx;
    return out.width * mmPerRectifiedPx;
}

} // namespace deltos::Geometry
