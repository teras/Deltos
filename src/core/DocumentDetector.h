#pragma once
#include "Quad.h"
#include <opencv2/dnn.hpp>
#include <mutex>
#include <optional>
#include <string>

namespace deltos {

// Finds the document quadrilateral in a photo.
// Runs the DocAligner heatmap model (if loaded) plus classic edge detection and
// keeps the best-scoring candidate.
class DocumentDetector {
public:
    struct Result {
        Quad quad;
        std::string method;       // model name, "edges" or "none"
        double confidence = 0;    // 0..1
    };

    // Loads the given ONNX model; empty or unloadable path means edges only.
    explicit DocumentDetector(const std::string& modelPath = {});

    bool hasModel() const { return !net_.empty(); }
    const std::string& modelName() const { return modelName_; }

    // Safe to call from several threads: cv::dnn::Net is not thread-safe, so calls are serialised.
    Result detect(const cv::Mat& bgr);

    std::optional<Quad> detectWithContours(const cv::Mat& bgr);

    bool modelWantsRgb = true;
    float heatmapThreshold = 0.3f;

private:
    struct Candidate { Quad quad; double confidence; std::string method; };

    std::optional<Candidate> runModel(const cv::Mat& bgr);
    bool plausible(const Quad& q, const cv::Size& s) const;

    cv::dnn::Net net_;
    std::string modelName_;
    std::mutex mutex_;
};

} // namespace deltos
