#!/bin/sh
# Builds the AppImage. Meant to run in a Debian bookworm container, which is old
# enough (glibc 2.36) for the result to run on anything current, and new enough
# to have Qt 6. OpenCV, Leptonica and Tesseract are built here because bookworm's
# OpenCV is 4.6, which cannot read the detection model.
#
#   docker run --rm --platform linux/amd64 -v "$PWD:/src:ro" -v "$PWD/out:/out" \
#       debian:bookworm sh /src/packaging/appimage/build.sh
#
# SRC is the source tree and OUT_DIR where the AppImage lands; both default to
# the mount points above, so the continuous build can run this in place.
set -eux

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkgconf git ca-certificates wget file desktop-file-utils \
    qt6-base-dev qt6-base-dev-tools qt6-wayland qt6-image-formats-plugins libgl1-mesa-dev \
    libheif-dev libpng-dev libjpeg-dev libtiff-dev libwebp-dev zlib1g-dev libarchive-dev

work=/build
mkdir -p "$work" && cd "$work"

fetch() { wget -q --tries=5 --waitretry=5 --timeout=60 -O "$2" "$1"; }

fetch https://github.com/DanBloomberg/leptonica/releases/download/1.87.0/leptonica-1.87.0.tar.gz lept.tar.gz
tar xf lept.tar.gz && cmake -S leptonica-1.87.0 -B lept-build -GNinja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
    -DBUILD_SHARED_LIBS=ON -DBUILD_PROG=OFF -DSW_BUILD=OFF
cmake --build lept-build && cmake --install lept-build

fetch https://github.com/tesseract-ocr/tesseract/archive/refs/tags/5.5.1.tar.gz tess.tar.gz
tar xf tess.tar.gz && cmake -S tesseract-5.5.1 -B tess-build -GNinja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
    -DBUILD_SHARED_LIBS=ON -DBUILD_TRAINING_TOOLS=OFF -DDISABLE_CURL=ON -DDISABLE_ARCHIVE=ON -DSW_BUILD=OFF
cmake --build tess-build && cmake --install tess-build

fetch https://github.com/opencv/opencv/archive/refs/tags/4.13.0.tar.gz opencv.tar.gz
tar xf opencv.tar.gz && cmake -S opencv-4.13.0 -B cv-build -GNinja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
    -DBUILD_LIST=core,imgproc,dnn -DBUILD_PROTOBUF=ON -DWITH_PROTOBUF=ON \
    -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF \
    -DBUILD_DOCS=OFF -DBUILD_JAVA=OFF -DWITH_FFMPEG=OFF -DWITH_GTK=OFF -DWITH_QT=OFF \
    -DWITH_OPENEXR=OFF -DWITH_GSTREAMER=OFF -DWITH_V4L=OFF -DOPENCV_GENERATE_PKGCONFIG=ON
cmake --build cv-build && cmake --install cv-build

cmake -S "${SRC:-/src}" -B app-build -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build app-build
DESTDIR="$work/AppDir" cmake --install app-build

# English and the orientation data travel with it: inside an AppImage there is
# no system Tesseract to ask, and the application looks here first.
mkdir -p "$work/AppDir/usr/share/deltos/tessdata"
for lang in eng osd; do
    fetch "https://github.com/tesseract-ocr/tessdata_best/raw/4.1.0/$lang.traineddata" \
          "$work/AppDir/usr/share/deltos/tessdata/$lang.traineddata"
done

fetch https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage linuxdeploy
fetch https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage linuxdeploy-plugin-qt
chmod +x linuxdeploy linuxdeploy-plugin-qt

# There is no FUSE in a container, so the tools unpack themselves instead.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=/usr/bin/qmake6
export EXTRA_QT_MODULES="waylandclient"
# linuxdeploy ships only xcb by default: a Wayland session would go through
# XWayland, and there would be no way to run the headless mode without a display.
export EXTRA_PLATFORM_PLUGINS="libqoffscreen.so;libqminimal.so;libqwayland-generic.so;libqwayland-egl.so"
export PATH="$work:$PATH"
export OUTPUT="${OUTPUT:-Deltos-x86_64.AppImage}"

./linuxdeploy --appdir AppDir --plugin qt --output appimage \
    --desktop-file AppDir/usr/share/applications/onl.ycode.Deltos.desktop \
    --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/onl.ycode.Deltos.png

out="${OUT_DIR:-/out}"
mkdir -p "$out" && mv -- *.AppImage "$out/"
ls -l "$out"
