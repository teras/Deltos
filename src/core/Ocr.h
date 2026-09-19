#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace deltos::Ocr {

struct Word {
    std::string text;
    cv::Rect box;         // pixels of the image handed to run()
    float confidence = 0; // 0..100
    int line = 0;         // words on the same text line share this
};

struct Result {
    std::vector<Word> words;  // reading order
    std::string text;         // the same words, joined by spaces and newlines
};

// Recognise the text of a rectified, already upright page. A language is a
// Tesseract spec: one or more traineddata names joined by '+'. Unlike the OCR
// inside Orientation::detect (which only scores rotations) this keeps what was
// read.
Result run(const cv::Mat& img, const std::string& language, const std::string& tessdataDir = {});

// traineddata installed on this machine, sorted; empty if Tesseract cannot say.
std::vector<std::string> availableLanguages(const std::string& tessdataDir = {});

// What to recognise in when the user has not chosen: the system language if its
// traineddata is installed, paired with English when that is installed too.
std::string defaultLanguage(const std::string& tessdataDir = {});

} // namespace deltos::Ocr
