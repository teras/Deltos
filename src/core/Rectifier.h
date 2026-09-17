#pragma once
#include "Quad.h"

namespace deltos {

namespace Rectifier {
// Warp the quad region of src into an upright image of the given size.
cv::Mat rectify(const cv::Mat& src, const Quad& q, const cv::Size& outSize);
}

} // namespace deltos
