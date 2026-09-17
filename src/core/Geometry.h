#pragma once
#include "Quad.h"
#include <optional>

namespace deltos {

namespace Geometry {

// Estimate width/height ratio of the real rectangle from its projected quad
// (Zhang & He, "Whiteboard scanning and image enhancement", MSR-TR-2003-39).
// focalPx: focal length in pixels; if <= 0, estimated from the quad itself when
// possible, otherwise max(w,h) is assumed.
// Returns width/height (>0). Falls back to the mean edge-length ratio on degenerate input.
double aspectRatio(const Quad& q, const cv::Size& imageSize, double focalPx = 0);

struct Standard {
    double ratio;          // width/height as output (may be the rotated form)
    double widthMm = 0;    // physical width matching `ratio`; 0 = unknown
    const char* name = ""; // "A4", "Letter", "ID-1", or ""
    const char* family = ""; // "A-series", "Letter", ...
};

// Snap to the closest standard paper ratio if within tolerance; otherwise returns
// the input ratio with widthMm = 0. Ratio families (A3/A4/A5 share √2) resolve to
// the member closest to measuredWidthMm when given, else to the common one (A4).
Standard snapToStandard(double ratio, double tolerance = 0.015, double measuredWidthMm = 0);

// Output size in pixels for the rectified image: width = longest of the
// horizontal edges, height derived from ratio.
cv::Size outputSize(const Quad& q, double ratio);

// Physical width of the document from the camera distance to its centre (pinhole):
// the local image scale at the centre of the plane, not the length of the nearest
// edge, is what corresponds to that distance.
double widthFromDistance(const Quad& q, double ratio, double distanceMm, double focalPx);

} // namespace Geometry
} // namespace deltos
