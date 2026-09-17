#pragma once
#include "DocumentDetector.h"
#include "Pipeline.h"
#include "Quad.h"
#include <optional>
#include <string>

namespace deltos {

// One photo through the whole pipeline: detection (unless the quad is known),
// reference-card search, rectification + enhancement, text orientation, rotation.
struct ScanInput {
    cv::Mat source;                // BGR, EXIF-oriented
    ProcessOptions options;        // referenceWidthMm is filled in by scanDocument
    std::optional<Quad> quad;      // known document quad; nullopt = detect
    std::optional<Quad> refQuad;   // known reference card; only used together with a known quad
    int autoRotation = -1;         // text orientation in degrees CW; -1 = detect
    int userRotation = 0;          // extra rotation on top, degrees CW
};

struct Scan {
    cv::Mat image;                 // final result, rotated
    Quad quad;
    std::string detection;         // method of the automatic detection, empty if the quad was given
    double confidence = 0;
    double aspect = 0;             // width/height of the document as seen by the camera
    std::optional<Quad> refQuad;
    double refWidthMm = 0;         // document width measured through the reference card
    bool swappable = false;        // document and reference both have the ID-1 shape
    int autoRotation = 0;
    double widthMm = 0;            // physical width of `image`
    Processed::SizeSource sizeSource = Processed::SizeSource::Unknown;
    const char* measuredBy = "";
    const char* standard = "";
};

Scan scanDocument(DocumentDetector& det, const ScanInput& in);

} // namespace deltos
