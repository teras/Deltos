# Developing Deltos

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    ./build/deltos [images...]

`cmake --install build` installs the binary, the detection model, the desktop
entry, the icons, the AppStream metadata and the manual page. On macOS the build
produces `Deltos.app` with the model inside it.

## Dependencies

| | |
|---|---|
| Qt 6.3 or newer | Widgets, Gui, Concurrent, Network |
| OpenCV 4.7 or newer, or 5 | core, imgproc, dnn; also `geometry` on OpenCV 5 |
| Tesseract 5 | with at least the `eng` and `osd` traineddata |
| libheif | optional, only for reading EXIF out of HEIC files |

Those two version numbers are floors, not preferences. Qt 6.3 is where
`QLocale::codeToLanguage` appears, which the language menu is built on. OpenCV
4.7 is the oldest that can read the detection model — 4.6 cannot parse the
opset-13 `Squeeze` the backbone is built from, and detection then falls back to
edges alone, which CMake warns about at configure time.

HEIC *decoding* comes from the Qt image-format plugins (kimageformats), not from
libheif; without them HEIC files cannot be opened at all. There is no such
plugin for Qt 6 in MSYS2, so the Windows build reads no HEIC — photographs sent
from an iPhone through the browser arrive as JPEG and are unaffected, but a
HEIC file opened directly will not load. Decoding it with libheif ourselves,
which is linked in already, would fix that everywhere at once.

OpenCV 5 moved `contourArea`, `approxPolyDP`, `getPerspectiveTransform` and
friends into a `geometry` module, so code that uses them includes
`core/CvCompat.h` rather than `<opencv2/imgproc.hpp>`, and CMake links that
module only on 5 and above. One source tree builds against either.

## How a photograph becomes a page

`core/Scan.cpp::scanDocument` is the single entry point, used by both the window
and the command line.

1. **Load.** EXIF orientation is applied, and the focal length, the Apple LiDAR
   depth and the standard subject distance are read out of the metadata.
2. **Detect.** The DocAligner ONNX model and classic Canny/contour detection run
   together; candidates are scored by the model's heatmap, the quadrilateral
   snaps to the edge one when at least three corners agree, and the model runs
   again on a crop when the document is small in the frame. The input is
   downscaled with `INTER_AREA` first — bilinear aliasing on text ruins the
   model's confidence.
3. **Shape.** The true width-to-height ratio comes from the four projected
   corners and the focal length in pixels, by the Zhang/He closed form. This is
   why the metadata matters: without a focal length the shape is only
   approximate.
4. **Size**, in order of preference: a manual override, then a reference ID-1
   card lying beside the document (~1 %), then the LiDAR distance (~15 %), then
   a standard paper shape within 1.5 %, then unknown. Which one it was is
   carried in `sizeSource` and shown in the window, and it must stay honest: the
   PDF page size is derived from the same number.
5. **Rectify and enhance.** The warp keeps the source pixel scale of the longest
   edge, so a 12 MP photograph of A4 gives roughly a 300 dpi scan.
6. **Orientation** runs on the *un-enhanced* rectified image: Tesseract OSD
   first, then an OCR-per-rotation fallback for sparse text such as cards. OSD's
   rotation direction is inverted relative to its own documentation — verified
   empirically, do not "fix" it.

The detection model is committed (`models/`, 80 MB, Apache 2.0) and is **not**
the upstream DocAligner file: its BiFPN weighted sums were rewritten as plain
multiplications and additions, because OpenCV 4 reads neither the `Einsum` they
used nor the `Relu` over the learnt weights feeding it. The computation is
unchanged bit for bit, and `models/rewrite-for-opencv4.py` reproduces the change
from the original. `core/Models.cpp::findModel` lists the places it is looked
for, the build tree and the install prefix among them.

## Text

Recognition is a separate pass, never part of `scanDocument`. It reads the
un-enhanced rectified image, so word boxes map one to one onto the result, and
it downscales only above ~3500 px on the long side — shrinking further loses the
small print, which is usually the part worth reading. The lines are then
reordered top-to-bottom and left-to-right within a band, because Tesseract emits
whole blocks in sequence and a column of labels can otherwise arrive before text
printed above it.

Languages are never hardcoded. The default pairs the system language with
English, since documents in any language carry amounts, codes and URLs in Latin
script. The menu keeps the codes in the order they were ticked, because
Tesseract treats the first as primary. Where the traineddata comes from is
`core/Tessdata.cpp`, in three steps: whatever Tesseract resolves by itself
(distribution packages, Homebrew, a Flatpak's prefix — asked for rather than
guessed), then what ships beside the executable, then what the user downloaded
through the language dialog. Tesseract accepts only one datapath, so the
directories are merged with symbolic links into the user's own, and the system's
copy of a language wins over ours.

Two things learnt the hard way: probe with `OEM_LSTM_ONLY`, because Debian,
Ubuntu and Fedora ship models with no legacy part at all and a legacy probe
finds nothing; and never call Tesseract with an empty language specification,
which it accepts before crashing inside `Recognize`.

## Photos from a phone

`net/LocalSendReceiver` speaks the LocalSend protocol (v2) and serves a browser
upload page, over minimal HTTP on QTcpServer, with multicast discovery on
224.0.0.167:53317. Files land in a per-process temporary directory. It listens
only while *Receive from phone* is on, accepts images only, and is GUI-only.

## Threading

Each page is processed in `QtConcurrent` with its own `QFutureWatcher`, and a
generation counter per row makes the latest job win. `cv::dnn::Net` is not
thread-safe, so `DocumentDetector::detect` serialises itself with a mutex.
Tesseract is single-threaded, so the orientation fallback runs eight independent
`TessBaseAPI` instances through `cv::parallel_for_`.

On Windows the same program is built twice: `deltos.exe` as a GUI application,
so that launching it never flashes a console, and `deltos-cli.exe` as a console
one, because a GUI binary detaches from the shell that started it -- the prompt
returns at once and anything printed goes nowhere.

## Packaging

`packaging/` holds a recipe per target — `arch/PKGBUILD`, `debian/`,
`fedora/deltos.spec`, `flatpak/onl.ycode.Deltos.yml` and `appimage/build.sh` —
and `.github/workflows/release.yml` builds all five on a tag, each inside a
container of its own distribution, then attaches them to the release. Pushing a
tag is the whole release procedure:

    git tag v1.1 && git push origin v1.1

Releases are published as prereleases; `gh release edit v1.1 --latest` promotes
one. The recipes require OpenCV 4.7 so that a package whose detector could never
load cannot be built by accident.

The two self-contained builds each carry their own OpenCV, Leptonica and
Tesseract, plus English and the orientation data; everything else is downloaded
at run time as usual. The Flatpak builds against `org.kde.Platform` 6.11. The
AppImage is built on Debian bookworm, the oldest base that still has Qt 6.3,
which fixes glibc at 2.36 and with it the machines it runs on.

## Verifying a change

There is no test suite. Verification is done with the command line on real
photographs: it prints the detection method and confidence, the aspect ratio,
the matched standard, the size and where it came from, and the orientation.

    ./build/deltos --no-gui --ocr tests/images/card.jpg

`DELTOS_DEBUG=dbg.jpg` draws the detected quadrilateral on the source,
`DELTOS_ORIENT_DEBUG=1` prints the score of each rotation the orientation
fallback tried, and `DELTOS_SCREENSHOT=/path.png` with
`QT_QPA_PLATFORM=offscreen` grabs the window without a display.

`docs/sample/` holds two printable A4 bills, one Greek and one English, with
invented names and amounts. They are monolingual on purpose, and carry Greek
capitals written without accents, six-point small print and a printed 100 mm
scale, so one photograph of one of them exercises the detector, the measured
size, the orientation and the recognition at once.

Photographs with personal data belong in `tests/images/private/`, which is not
committed.
