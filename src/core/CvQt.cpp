#include "CvQt.h"
#include <opencv2/imgproc.hpp>

namespace deltos {

QImage matToQImage(const cv::Mat& m) {
    if (m.empty()) return {};
    if (m.type() == CV_8UC1)
        return QImage(m.data, m.cols, m.rows, int(m.step), QImage::Format_Grayscale8).copy();
    cv::Mat rgb;
    if (m.type() == CV_8UC3) cv::cvtColor(m, rgb, cv::COLOR_BGR2RGB);
    else if (m.type() == CV_8UC4) cv::cvtColor(m, rgb, cv::COLOR_BGRA2RGB);
    else return {};
    return QImage(rgb.data, rgb.cols, rgb.rows, int(rgb.step), QImage::Format_RGB888).copy();
}

cv::Mat qImageToMat(const QImage& img) {
    QImage rgb = img.convertToFormat(QImage::Format_RGB888);
    cv::Mat tmp(rgb.height(), rgb.width(), CV_8UC3, const_cast<uchar*>(rgb.bits()), size_t(rgb.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(tmp, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

} // namespace deltos
