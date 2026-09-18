# Deltos

<img src="icons/deltos.svg" width="96" align="right" alt="">

Desktop document scanner: take a photo of a document at any angle, get a flat,
upright, correctly-proportioned page and export it as a PDF, PNG or JPG with
real physical dimensions. Works as a GUI and from the command line.

## Input and output

**Input**: any image format the installed Qt image plugins can read — JPEG, PNG,
TIFF, WebP, BMP, and HEIC/HEIF when the kimageformats plugin
is present. EXIF orientation is applied on load, and EXIF metadata is used by
the pipeline (see below). Open files with the toolbar button, on the command
line, by dragging them onto the window, or straight from a phone (see below).

**Output**, from the *Export* dropdown:

- **PDF, all pages** — one page per document, each page sized to its own
  physical dimensions (e.g. an A4 sheet becomes a 210 × 297 mm page, an ID card
  an 85.6 × 54 mm page). Black & white pages are stored as lossless 1-bit
  images; the rest are JPEG-compressed.
- **PDF, current page** — the same for the selected document only.
- **PNG / JPG, current page** — the processed image, with the physical size
  embedded as DPI so other tools show it at scale. Black & white PNGs are 1-bit.

The output keeps the source pixel scale of the document's longest edge, so a
12 MP photo of an A4 sheet gives roughly a 300 dpi scan.

## From a phone

Photos must arrive with their metadata intact: the focal length gives the true
page shape and, on iPhones with LiDAR, the camera distance gives the physical
size without a reference card. Messaging apps (WhatsApp, Messenger, mail)
strip that and shrink the image. Deltos therefore receives photos directly over
the local network: turn on *Open → Receive from phone*. Two ways:

- **Browser**: scan the QR code with the phone's camera, or type the address
  (e.g. `http://192.168.1.5:53317/`) in its browser, and pick the photos.
  Nothing to install. iOS uploads photos as full-resolution JPEG with the
  metadata intact (WebKit converts HEIC on upload); that loses nothing Deltos
  needs.
- **LocalSend app**: with the free, open-source [LocalSend](https://localsend.org)
  app (iOS and Android), share the photos and pick "Deltos (hostname)" from the
  device list. This is the only way to get the original HEIC untouched.

Both use port 53317 (TCP; plus UDP multicast for LocalSend discovery). If the
phone cannot connect, open that port in the computer's firewall. If the port
is taken (the LocalSend desktop app is running) Deltos takes the next free one
and shows it, so both can run together as long as that port is open too. The
receiver is plain HTTP on the local network, accepts image files only, and
listens only while *Receive from phone* is on.

## What it does

- **Detection**: the DocAligner ONNX model plus classic edge detection, with a
  coarse-to-fine pass for documents that are small in the frame. Corners
  can be dragged by hand in the editor; *Detect again* reruns detection.
- **Geometry**: the true aspect ratio of the page is recovered from the four
  corners and the camera focal length (EXIF), so the output is not just
  "rectangular" but the right shape. Without EXIF a 28 mm-equivalent lens is
  assumed.
- **Physical size**, in order of preference: user override → reference credit
  card lying beside the document (~1 %) → camera distance from iPhone LiDAR
  metadata (~15 %) → recognised standard shape (A-series, Letter, Legal, ID-1,
  DL) → unknown. The size and how it was obtained are shown under the result;
  the *Size* box lets you force a standard (A4, A5, A3, Letter, Legal, ID card,
  DL) or type a custom width.
- **Reference card**: put any ID-1 card (credit card, ID, driving licence) next
  to the document and the scale is measured from it. If the document itself is
  card-sized, *Swap document ↔ reference* picks the other object.
- **Orientation**: Tesseract OSD, with an OCR-per-rotation fallback for sparse
  text such as cards. *Rotate 90°* fixes it by hand.
- **Enhancement**: *Color* (shadow lift and paper white balance — dark objects
  stay dark), *Grayscale* and *Black & white*, with a strength slider (in B&W
  mode it sets how aggressively faint ink is dropped).

## Dependencies

Qt 6 (Widgets, Gui, Concurrent, Network), OpenCV 4 or 5 (core, imgproc, imgcodecs, dnn;
`geometry` on OpenCV 5), Tesseract 5 with `osd`, `ell`, `eng` traineddata,
libheif (optional, for EXIF in HEIC). HEIC decoding itself comes from the Qt
image-format plugins (kimageformats).

The detection model (`models/fastvit_sa24_h_e_bifpn_256_fp32.onnx`, 83 MB,
Apache 2.0, from [DocAligner](https://github.com/DocsaidLab/DocAligner)) is part
of the repository and of every build. At run time it is looked up, in this
order, in the first place it exists:

1. `models/` next to the executable (the build tree)
2. `../models` relative to the executable
3. `../Resources/models` (inside `Deltos.app`)
4. `../share/deltos/models` (Linux install prefix)
5. the per-user application data directory: `~/.local/share/deltos/models`
   on Linux, `~/Library/Application Support/deltos/models` on macOS,
   `%APPDATA%\deltos\models` on Windows
6. `models/` in the current directory

A different DocAligner model with the same interface can be given with
`--model FILE`; without any model detection falls back to edges only.

## Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    ./build/deltos [images...]

On Linux `cmake --install build` installs the binary, the model, the `.desktop`
file and the icons. On macOS the build produces `Deltos.app` with the model
inside.

## Command line

    deltos [options] image...

Without `--no-gui` the images open in the window; every option below also
applies there (as the initial setting of each page, the detector, or the
export), except `--out`, which is ignored with a warning.

      --no-gui          run headless
      --out FILE        write the result: .pdf takes any number of images (one page each),
                        .png/.jpg take exactly one; without --out only the analysis is printed
      --mode MODE       color (default), gray or bw
      --strength 0-100  enhancement strength (default 50)
      --model FILE      DocAligner ONNX model to use (default: the shipped one)
      --no-model        edge detection only
      --focal PX        camera focal length in pixels (default: from EXIF)
      --self-focal      estimate the focal length from the image instead of EXIF
      --no-snap         do not snap the aspect ratio to a standard paper size
      --dpi N           page density for images of unknown physical size (default 300)

Headless, for every image it prints the detection method and confidence, the
corner coordinates, the aspect ratio and matched standard, the physical size and
how it was obtained, and the text orientation. `DELTOS_DEBUG=FILE` additionally
saves the source with the detected quad drawn on it (`-1`, `-2`, ... is added
before the extension when there are several images).

## License

GPL-3.0-only, see `LICENSE`. The detection model is Apache 2.0 (`models/LICENSE`)
and the bundled QR code generator (`third_party/qrcodegen`, Project Nayuki) is MIT.
