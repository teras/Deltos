#include "Pipeline.h"
#include "Geometry.h"
#include "Rectifier.h"
#include <cstdio>
#include <cstdlib>

namespace deltos {

Processed processDocument(const cv::Mat& bgr, const Quad& q, const ProcessOptions& opt) {
    Processed out;
    double ratio = Geometry::aspectRatio(q, bgr.size(), opt.focalPx < 0 ? 0 : opt.focalPx);

    // Best available measurement: a reference card (~1%) beats camera distance (~15%).
    double measuredMm = 0;
    if (opt.referenceWidthMm > 0) {
        measuredMm = opt.referenceWidthMm;
        out.measuredBy = "reference card";
    } else if (opt.subjectDistanceMm > 0 && opt.focalPx > 0) {
        measuredMm = Geometry::widthFromDistance(q, ratio, opt.subjectDistanceMm, opt.focalPx);
        out.measuredBy = "camera distance";
    }

    if (getenv("DELTOS_DEBUG") && measuredMm > 0) fprintf(stderr, "measured width: %.1f mm (%s)\n", measuredMm, out.measuredBy);
    if (opt.snapAspect) {
        const auto std = Geometry::snapToStandard(ratio, 0.015, measuredMm);
        if (std.widthMm > 0) {
            ratio = std.ratio;
            out.widthMm = std.widthMm;
            out.standard = std.name;
            out.sizeSource = measuredMm > 0 ? Processed::SizeSource::Verified : Processed::SizeSource::Assumed;
        }
    }
    if (out.widthMm <= 0 && measuredMm > 0) {
        out.widthMm = measuredMm;
        out.sizeSource = Processed::SizeSource::Measured;
    }
    if (opt.manualWidthMm > 0) {
        out.widthMm = opt.manualWidthMm;
        out.sizeSource = Processed::SizeSource::Manual;
    }
    const cv::Size size = Geometry::outputSize(q, ratio);
    out.rectified = Rectifier::rectify(bgr, q, size);
    out.image = Enhancer::enhance(out.rectified, opt.mode, opt.strength);
    return out;
}

} // namespace deltos
