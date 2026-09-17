#pragma once
#include <opencv2/core.hpp>
#include <string>

namespace deltos::Orientation {

struct Result {
    int degreesCW = 0;      // rotation to apply (clockwise) so text reads upright: 0/90/180/270
    double confidence = 0;  // Tesseract OSD orientation confidence
    bool ok = false;
};

// Detect text orientation with Tesseract OSD on a rectified page.
// tessdataDir may be empty to use the default search path.
Result detect(const cv::Mat& img, const std::string& tessdataDir = {});

} // namespace deltos::Orientation
