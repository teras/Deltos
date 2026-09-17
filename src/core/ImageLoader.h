#pragma once
#include <opencv2/core.hpp>
#include <string>

namespace deltos {

struct LoadedImage {
    cv::Mat bgr;        // EXIF-oriented
    double focalPx = 0; // from EXIF, or a default estimate
    bool hasExif = false;
    int focalLength35mm = 0;
    double subjectDistanceMm = 0; // from LiDAR/ToF metadata when available
    std::string error;
};

// Loads any format Qt can read (JPEG, PNG, HEIC via plugin, ...), applies EXIF orientation.
LoadedImage loadImage(const std::string& path);

} // namespace deltos
