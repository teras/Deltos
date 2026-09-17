#include "ReferenceCard.h"
#include "CvCompat.h"
#include "Geometry.h"

namespace deltos {

namespace {

struct Plane {
    cv::Mat H, Hinv;      // source -> plane, plane -> source
    cv::Rect footprint;   // plane region covered by the photo (clamped)
    cv::Rect doc;         // document rectangle in plane coordinates
};

// Homography that maps the document quad to an upright rectangle of its output
// size; the rest of the photo lands on the same metric plane (up to scale).
Plane planeFor(const cv::Mat& bgr, const Quad& q, double ratio) {
    const cv::Size docSize = Geometry::outputSize(q, ratio);
    const int mx = docSize.width * 2, my = docSize.height * 2;
    const cv::Point2f dst[4] = {
        {float(mx), float(my)}, {float(mx + docSize.width), float(my)},
        {float(mx + docSize.width), float(my + docSize.height)}, {float(mx), float(my + docSize.height)}};
    cv::Mat H = cv::getPerspectiveTransform(q.pts.data(), dst);

    std::vector<cv::Point2f> corners = {{0, 0}, {float(bgr.cols), 0}, {float(bgr.cols), float(bgr.rows)}, {0, float(bgr.rows)}}, pc;
    cv::perspectiveTransform(corners, pc, H);
    cv::Rect fp = cv::boundingRect(pc) & cv::Rect(0, 0, docSize.width * 5, docSize.height * 5);
    const cv::Mat T = (cv::Mat_<double>(3, 3) << 1, 0, -fp.x, 0, 1, -fp.y, 0, 0, 1);
    Plane p;
    p.H = T * H;
    p.Hinv = p.H.inv();
    p.footprint = cv::Rect(0, 0, fp.width, fp.height);
    p.doc = cv::Rect(mx - fp.x, my - fp.y, docSize.width, docSize.height);
    return p;
}

// Mean of opposite edge pairs of a rectangle-ish quad; returns (long, short).
std::pair<double, double> sides(const Quad& q) {
    const double w = (cv::norm(q[1] - q[0]) + cv::norm(q[2] - q[3])) / 2;
    const double h = (cv::norm(q[3] - q[0]) + cv::norm(q[2] - q[1])) / 2;
    return {std::max(w, h), std::min(w, h)};
}

Quad transformQuad(const Quad& q, const cv::Mat& H) {
    std::vector<cv::Point2f> in(q.pts.begin(), q.pts.end()), out;
    cv::perspectiveTransform(in, out, H);
    Quad r;
    std::copy(out.begin(), out.end(), r.pts.begin());
    return r;
}

ReferenceResult resultFor(const Plane& p, const Quad& refPlane, const Quad& refSource, double confidence) {
    const auto [lon, sho] = sides(refPlane);
    const double target = kId1WidthMm / kId1HeightMm;
    ReferenceResult r;
    r.quad = refSource;
    r.ratioError = std::abs(lon / sho - target) / target;
    r.docWidthMm = p.doc.width * (kId1WidthMm / lon);
    r.confidence = confidence;
    return r;
}

} // namespace

std::optional<ReferenceResult> findReferenceCard(DocumentDetector& det, const cv::Mat& bgr,
                                                 const Quad& docQuad, double docRatio) {
    if (bgr.empty() || !det.hasModel()) return std::nullopt;
    const Plane p = planeFor(bgr, docQuad, docRatio);
    if (p.footprint.width < 50 || p.footprint.height < 50) return std::nullopt;

    cv::Mat plane;
    cv::warpPerspective(bgr, plane, p.H, p.footprint.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    // The document cannot be in the strips around it; the card, lying beside it, is.
    const int pad = int(0.04 * std::max(p.doc.width, p.doc.height));
    const cv::Rect outer = (p.doc + cv::Point(-pad, -pad) + cv::Size(2 * pad, 2 * pad)) & p.footprint;
    const cv::Rect strips[] = {
        cv::Rect(0, 0, outer.x, plane.rows),
        cv::Rect(outer.br().x, 0, plane.cols - outer.br().x, plane.rows),
        cv::Rect(0, 0, plane.cols, outer.y),
        cv::Rect(0, outer.br().y, plane.cols, plane.rows - outer.br().y),
    };

    std::optional<ReferenceResult> best;
    for (cv::Rect st : strips) {
        st &= p.footprint;
        if (st.width < 50 || st.height < 50) continue;
        auto r = det.detect(plane(st));
        if (r.method == "none") continue;
        Quad qPlane = r.quad;
        for (auto& pt : qPlane.pts) pt += cv::Point2f(float(st.x), float(st.y));
        const Quad qSource = transformQuad(qPlane, p.Hinv);
        ReferenceResult cand = resultFor(p, qPlane, qSource, r.confidence);
        if (cand.ratioError > 0.03) continue;
        if (!best || cand.ratioError < best->ratioError) best = cand;
    }
    return best;
}

std::optional<ReferenceResult> measureWithReference(const cv::Mat& bgr, const Quad& docQuad,
                                                    double docRatio, const Quad& refQuad) {
    if (bgr.empty()) return std::nullopt;
    const Plane p = planeFor(bgr, docQuad, docRatio);
    const Quad refPlane = transformQuad(refQuad, p.H);
    const auto [lon, sho] = sides(refPlane);
    if (lon < 1 || sho < 1) return std::nullopt;
    return resultFor(p, refPlane, refQuad, 1.0);
}

} // namespace deltos
