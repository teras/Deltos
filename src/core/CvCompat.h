#pragma once
// OpenCV 5 moved contour/shape functions (contourArea, approxPolyDP, moments,
// getPerspectiveTransform, ...) from imgproc to the geometry module.
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry.hpp>
#endif
