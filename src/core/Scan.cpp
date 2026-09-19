#include "Scan.h"
#include "Geometry.h"
#include "Orientation.h"
#include "ReferenceCard.h"
#include <opencv2/imgproc.hpp>

namespace deltos {

Scan scanDocument(DocumentDetector& det, const ScanInput& in) {
    Scan s;
    ProcessOptions opt = in.options;
    const double focal = opt.focalPx < 0 ? 0 : opt.focalPx;
    s.refQuad = in.refQuad;
    const bool detect = !in.quad;
    if (detect) {
        auto r = det.detect(in.source);
        s.quad = r.quad;
        s.detection = r.method;
        s.confidence = r.confidence;
        s.refQuad.reset();
    } else {
        s.quad = *in.quad;
    }
    s.aspect = Geometry::aspectRatio(s.quad, in.source.size(), focal);
    const double docRatio = Geometry::snapToStandard(s.aspect).ratio;
    if (detect && s.detection != "none") {
        if (auto ref = findReferenceCard(det, in.source, s.quad, docRatio)) {
            s.refQuad = ref->quad;
            s.refWidthMm = ref->docWidthMm;
        }
    } else if (s.refQuad) {
        // Quad may have been edited or swapped: re-measure with the known reference.
        if (auto ref = measureWithReference(in.source, s.quad, docRatio, *s.refQuad))
            s.refWidthMm = ref->docWidthMm;
    }
    if (s.refQuad) {
        // Which one is the document is only ambiguous when both have the card shape.
        const double lon = std::max(s.aspect, 1 / s.aspect), id1 = kId1WidthMm / kId1HeightMm;
        s.swappable = std::abs(lon - id1) / id1 < 0.03;
    }
    opt.referenceWidthMm = s.refWidthMm;

    Processed pr = processDocument(in.source, s.quad, opt);
    s.image = pr.image;
    s.rectified = pr.rectified;
    s.widthMm = pr.widthMm;
    s.sizeSource = pr.sizeSource;
    s.measuredBy = pr.measuredBy;
    s.standard = pr.standard;

    s.autoRotation = in.autoRotation;
    if (s.autoRotation < 0) {
        // Text orientation is detected once per page, on the un-enhanced rectified image.
        auto o = Orientation::detect(pr.rectified);
        s.autoRotation = o.ok ? o.degreesCW : 0;
    }
    const int rot = (s.autoRotation + in.userRotation) % 360;
    if (rot == 90 || rot == 180 || rot == 270) {
        const auto code = rot == 90 ? cv::ROTATE_90_CLOCKWISE : rot == 180 ? cv::ROTATE_180 : cv::ROTATE_90_COUNTERCLOCKWISE;
        cv::rotate(s.image, s.image, code);
        cv::rotate(s.rectified, s.rectified, code);
    }
    // widthMm describes the unrotated width; after a 90/270 turn it becomes the height.
    if (s.widthMm > 0 && (rot == 90 || rot == 270)) s.widthMm = s.widthMm * s.image.cols / s.image.rows;
    return s;
}

} // namespace deltos
