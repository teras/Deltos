#include "Rectifier.h"
#include "CvCompat.h"

namespace deltos::Rectifier {

cv::Mat rectify(const cv::Mat& src, const Quad& q, const cv::Size& outSize) {
    const cv::Point2f dst[4] = {
        {0.f, 0.f}, {float(outSize.width), 0.f},
        {float(outSize.width), float(outSize.height)}, {0.f, float(outSize.height)}};
    const cv::Mat M = cv::getPerspectiveTransform(q.pts.data(), dst);
    cv::Mat out;
    cv::warpPerspective(src, out, M, outSize, cv::INTER_CUBIC, cv::BORDER_REPLICATE);
    return out;
}

} // namespace deltos::Rectifier
