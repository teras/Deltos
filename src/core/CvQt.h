#pragma once
#include <QImage>
#include <opencv2/core.hpp>

namespace deltos {
// Deep-copy conversions. cv::Mat is BGR (3ch) or gray (1ch).
QImage matToQImage(const cv::Mat& m);
cv::Mat qImageToMat(const QImage& img);   // -> BGR CV_8UC3
}
