#pragma once
#include "DocumentDetector.h"
#include "Quad.h"
#include <optional>

namespace deltos {

// ID-1 card (credit/ID card) dimensions — the same everywhere in the world.
constexpr double kId1WidthMm = 85.60, kId1HeightMm = 53.98;

struct ReferenceResult {
    Quad quad;              // reference card in source image coordinates
    double docWidthMm = 0;  // width of the document derived from the card
    double ratioError = 0;  // |ratio - 1.586| / 1.586 in the rectified plane
    double confidence = 0;
};

// Rectifies the whole plane of the document and searches the four strips around
// the document for a second object with the ID-1 aspect ratio.
std::optional<ReferenceResult> findReferenceCard(DocumentDetector& det, const cv::Mat& bgr,
                                                 const Quad& docQuad, double docRatio);

// Same measurement, but with a known reference quad (e.g. after the user swapped
// document and reference). Returns nullopt only on degenerate input.
std::optional<ReferenceResult> measureWithReference(const cv::Mat& bgr, const Quad& docQuad,
                                                    double docRatio, const Quad& refQuad);

} // namespace deltos
