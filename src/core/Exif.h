#pragma once
#include <optional>
#include <string>

namespace deltos::Exif {

struct Info {
    double focalLengthMm = 0;     // 0x920A
    int focalLength35mm = 0;      // 0xA405
    int orientation = 1;          // 0x0112
    double subjectDistanceMm = 0;     // best available camera-to-subject distance; 0 = unknown
    double exifSubjectDistanceMm = 0; // standard EXIF 0x9206 (rarely written by phones)
};

// Minimal JPEG APP1/EXIF reader. Returns nullopt if no EXIF present.
std::optional<Info> read(const std::string& path);

// Focal length in pixels for an image of the given (already oriented) size.
// Uses 35mm-equivalent if available, else a 28mm-equivalent default.
double focalPx(const std::optional<Info>& info, int width, int height);

} // namespace deltos::Exif
