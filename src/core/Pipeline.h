#pragma once
#include "DocumentDetector.h"
#include "Enhancer.h"

namespace deltos {

struct ProcessOptions {
    ColorMode mode = ColorMode::Color;
    double strength = 0.5;   // see Enhancer::enhance
    bool snapAspect = true;
    double focalPx = 0;   // >0 = known (e.g. from EXIF), <=0 = self-estimate with fallback
    double subjectDistanceMm = 0; // camera-to-document distance if known (LiDAR); enables a size estimate
    double referenceWidthMm = 0;  // document width measured via a reference card, 0 = none
    double manualWidthMm = 0;     // user override of the output width, 0 = none
};

struct Processed {
    cv::Mat image;
    cv::Mat rectified;    // warped but un-enhanced (BGR); stable input for orientation detection
    double widthMm = 0;   // physical width, see sizeSource
    enum class SizeSource { Unknown, Assumed, Measured, Verified, Manual } sizeSource = SizeSource::Unknown;
    //   Assumed  = shape matches a standard, size taken on faith (no measurement)
    //   Measured = measured (reference card or camera distance), no standard matched
    //   Verified = shape matches a standard and the measurement agrees
    //   Manual   = user override
    const char* measuredBy = "";  // "reference card" or "camera distance" when a measurement was used
    const char* standard = "";
};

// Rectify + enhance a source image using a known quad.
Processed processDocument(const cv::Mat& bgr, const Quad& q, const ProcessOptions& opt);

} // namespace deltos
