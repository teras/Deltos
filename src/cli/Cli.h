#pragma once
#include "core/Ocr.h"
#include "core/Pipeline.h"
#include "export/PdfExporter.h"
#include <optional>
#include <string>
#include <vector>

namespace deltos {

// Command line of both modes; see usage() in Cli.cpp.
struct Options {
    bool noGui = false;
    std::string model;              // ONNX model path, empty = edge detection only
    bool rgb = true;                // model input channel order
    ProcessOptions process;         // mode, strength, snapAspect, focalPx (0 = EXIF, -1 = self-estimate)
    PdfOptions pdf;
    bool ocr = false;               // recognise the text: printed headless, shown as boxes in the GUI
    std::string ocrLanguage;        // --ocr-lang; empty = saved setting, else the system language
    std::string out;                // output file, headless only
    std::vector<std::string> inputs;
};

// Parses argv; prints usage and returns nullopt on any error.
std::optional<Options> parseOptions(int argc, char** argv);

// Headless mode.
int runCli(const Options& opt);

} // namespace deltos
