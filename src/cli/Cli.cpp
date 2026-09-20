#include "Cli.h"
#include "core/CvQt.h"
#include "core/ImageLoader.h"
#include "core/Models.h"
#include "core/Ocr.h"
#include "core/Settings.h"
#include "core/Scan.h"
#include "export/ImageExporter.h"
#include "export/PdfExporter.h"
#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace deltos {

static void usage() {
    std::cerr <<
        "usage: deltos [options] image...\n"
        "  --no-gui          run headless; without it the images open in the window\n"
        "  --out FILE        write the result: .pdf takes any number of images (one page each),\n"
        "                    .png/.jpg take exactly one; without --out only the analysis is printed\n"
        "  --mode MODE       color (default), gray or bw\n"
        "  --strength 0-100  enhancement strength (default 50)\n"
        "  --model FILE      DocAligner ONNX model to use (default: the shipped one)\n"
        "  --no-model        edge detection only\n"
        "  --focal PX        camera focal length in pixels (default: from EXIF)\n"
        "  --self-focal      estimate the focal length from the image instead of EXIF\n"
        "  --no-snap         do not snap the aspect ratio to a standard paper size\n"
        "  --ocr             also read the text of each page (printed here, selectable in the GUI)\n"
        "  --ocr-lang SPEC   tesseract language(s) for --ocr, '+'-joined; default is the language\n"
        "                    last picked in the GUI, else the system language plus English\n"
        "  --bgr             feed the model BGR instead of RGB\n"
        "  --dpi N           page density for images of unknown physical size (default 300)\n"
        "--out only applies with --no-gui; every other option applies to both modes.\n"
        "DELTOS_DEBUG=FILE draws the detected quad on the source and saves it there\n"
        "(with several images, -1, -2, ... is added before the extension).\n";
}

static QString outputFormat(const std::string& out) {
    const QString ext = QFileInfo(QString::fromStdString(out)).suffix().toLower();
    return ext == "jpeg" ? "jpg" : ext;
}

std::optional<Options> parseOptions(int argc, char** argv) {
    Options o;
    o.model = findModel();
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool hasValue = i + 1 < argc;
        if (a == "--no-gui") o.noGui = true;
        else if (a == "--out" && hasValue) o.out = argv[++i];
        else if (a == "--model" && hasValue) o.model = argv[++i];
        else if (a == "--mode" && hasValue) {
            const std::string m = argv[++i];
            o.process.mode = m == "gray" ? ColorMode::Gray : m == "bw" ? ColorMode::BlackWhite : ColorMode::Color;
        }
        else if (a == "--strength" && hasValue) o.process.strength = std::stod(argv[++i]) / 100.0;
        else if (a == "--focal" && hasValue) o.process.focalPx = std::stod(argv[++i]);
        else if (a == "--dpi" && hasValue) o.pdf.dpi = std::stoi(argv[++i]);
        else if (a == "--self-focal") o.process.focalPx = -1;
        else if (a == "--no-model") o.model.clear();
        else if (a == "--no-snap") o.process.snapAspect = false;
        else if (a == "--ocr") o.ocr = true;
        else if (a == "--ocr-lang" && hasValue) { o.ocrLanguage = argv[++i]; o.ocr = true; }
        else if (a == "--bgr") o.rgb = false;
        else if (a.rfind("--", 0) == 0) { usage(); return std::nullopt; }
        else o.inputs.push_back(a);
    }
    if (o.ocrLanguage.empty())
        o.ocrLanguage = QSettings(settingsPath(), QSettings::IniFormat)
                            .value("ocr/language", QString::fromStdString(Ocr::defaultLanguage()))
                            .toString().toStdString();
    if (!o.noGui && !o.out.empty()) {
        std::cerr << "--out is ignored without --no-gui\n";
        o.out.clear();
    }
    if (o.noGui) {
        const QString format = outputFormat(o.out);
        if (o.inputs.empty() || (!o.out.empty() && format != "pdf" && format != "png" && format != "jpg")) {
            usage();
            return std::nullopt;
        }
        if ((format == "png" || format == "jpg") && o.inputs.size() != 1) {
            std::cerr << "an image output takes exactly one input\n";
            return std::nullopt;
        }
    }
    return o;
}

// dbg.jpg -> dbg-3.jpg for the third of several inputs.
static std::string numbered(const std::string& path, size_t index, size_t total) {
    if (total < 2) return path;
    const QFileInfo fi(QString::fromStdString(path));
    const QString suffix = fi.suffix().isEmpty() ? QString() : "." + fi.suffix();
    return (fi.path() + "/" + fi.completeBaseName() + "-" + QString::number(index + 1) + suffix).toStdString();
}

int runCli(const Options& opt) {
    const QString outPath = QString::fromStdString(opt.out);
    const QString format = outputFormat(opt.out);
    const std::string debug = qEnvironmentVariable("DELTOS_DEBUG").toStdString();
    DocumentDetector det(opt.model);
    det.modelWantsRgb = opt.rgb;
    std::vector<PdfPage> pages;
    for (size_t n = 0; n < opt.inputs.size(); ++n) {
        const std::string& path = opt.inputs[n];
        LoadedImage li = loadImage(path);
        if (li.bgr.empty()) { std::cerr << "cannot read " << path << ": " << li.error << "\n"; return 1; }
        ScanInput in;
        in.source = li.bgr;
        in.options = opt.process;
        if (opt.process.focalPx == 0) in.options.focalPx = li.focalPx;
        in.options.subjectDistanceMm = li.subjectDistanceMm;
        std::cout << path << ": " << li.bgr.cols << "x" << li.bgr.rows
                  << " exif=" << (li.hasExif ? "yes" : "no") << " f35=" << li.focalLength35mm
                  << " dist=" << li.subjectDistanceMm << "mm -> f=" << in.options.focalPx << "px\n";
        const Scan s = scanDocument(det, in);
        std::cout << "  detection: " << s.detection << " (" << int(s.confidence * 100) << "%) quad=";
        for (const auto& p : s.quad.pts) std::cout << "(" << p.x << "," << p.y << ") ";
        std::cout << "\n  aspect: " << s.aspect << (s.standard[0] ? std::string(" -> ") + s.standard : "") << "\n";
        if (s.refQuad) std::cout << "  reference card: doc width " << s.refWidthMm << " mm\n";
        if (s.widthMm > 0) {
            static const char* src[] = {"unknown", "assumed", "measured", "verified", "manual"};
            std::cout << "  size: " << s.widthMm << " x " << s.widthMm * s.image.rows / s.image.cols << " mm ("
                      << src[int(s.sizeSource)] << (s.measuredBy[0] ? std::string(", ") + s.measuredBy : "") << ")\n";
        }
        std::cout << "  orientation: " << s.autoRotation << " deg, output " << s.image.cols << "x" << s.image.rows << "\n";
        if (opt.ocr) {
            const Ocr::Result o = Ocr::run(s.rectified, opt.ocrLanguage);
            std::cout << "  ocr [" << opt.ocrLanguage << "]: " << o.words.size() << " words\n";
            if (!o.text.empty()) std::cout << o.text << "\n";
        }

        if (!debug.empty()) {
            cv::Mat d = li.bgr.clone();
            const int thick = std::max(2, d.cols / 300);
            for (int i = 0; i < 4; ++i) {
                cv::line(d, s.quad[i], s.quad[(i + 1) % 4], {0, 0, 255}, thick);
                cv::putText(d, std::to_string(i), s.quad[i], cv::FONT_HERSHEY_SIMPLEX, d.cols / 800.0, {0, 255, 0}, 2);
            }
            if (s.refQuad)
                for (int i = 0; i < 4; ++i) cv::line(d, (*s.refQuad)[i], (*s.refQuad)[(i + 1) % 4], {255, 128, 0}, thick);
            // Saved through Qt, which is already here for everything else:
            // OpenCV's imgcodecs would drag in a stack of image and video
            // codecs for this one debugging line.
            matToQImage(d).save(QString::fromStdString(numbered(debug, n, opt.inputs.size())));
        }
        if (format == "pdf") pages.push_back(pdfPage(s.image, opt.process.mode, s.widthMm));
        else if (!format.isEmpty() && !saveImage(outPath, format, s.image, opt.process.mode, s.widthMm)) {
            std::cerr << "cannot write " << opt.out << "\n";
            return 1;
        }
    }
    if (format == "pdf") {
        QString err;
        if (!exportPdf(outPath, pages, opt.pdf, &err)) { std::cerr << err.toStdString() << "\n"; return 1; }
    }
    if (!opt.out.empty()) std::cout << "wrote " << opt.out << "\n";
    return 0;
}

} // namespace deltos
