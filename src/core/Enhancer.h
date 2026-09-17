#pragma once
#include <opencv2/core.hpp>

namespace deltos {

enum class ColorMode { Color, Gray, BlackWhite };

namespace Enhancer {
// strength in [0,1]:
//   Color/Gray: 0 = untouched, 1 = full shadow lift / white balance / sharpening (blend in between)
//   BlackWhite: threshold cleanliness, 0 = keep faint ink (and noise), 1 = only strong ink
cv::Mat enhance(const cv::Mat& bgr, ColorMode mode, double strength = 0.5);
}

} // namespace deltos
