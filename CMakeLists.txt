cmake_minimum_required(VERSION 3.21)
project(Deltos VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Concurrent Network)
# No imgcodecs: images come and go through Qt, and OpenCV's codec module
# drags in a stack of image and video codecs nothing here decodes.
find_package(OpenCV REQUIRED COMPONENTS core imgproc dnn)
if(OpenCV_VERSION VERSION_GREATER_EQUAL 5)
    find_package(OpenCV REQUIRED COMPONENTS geometry)
elseif(OpenCV_VERSION VERSION_LESS 4.7)
    # Measured: 4.7 reads the model, 4.6 (Ubuntu 24.04) does not -- it cannot
    # parse the opset-13 Squeeze the backbone is built from. Everything still
    # builds and runs, but the document is found by edge detection alone.
    message(WARNING "OpenCV ${OpenCV_VERSION} is too old to load the detection model; "
                    "detection will fall back to edges only. 4.7 or newer is needed.")
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(Tesseract REQUIRED IMPORTED_TARGET tesseract)
pkg_check_modules(HEIF IMPORTED_TARGET libheif)

# Core: pure OpenCV, no Qt widgets
add_library(deltos_core STATIC
    src/core/Quad.h
    src/core/Models.h
    src/core/CvCompat.h
    src/core/DocumentDetector.h src/core/DocumentDetector.cpp
    src/core/Geometry.h src/core/Geometry.cpp
    src/core/Rectifier.h src/core/Rectifier.cpp
    src/core/Enhancer.h src/core/Enhancer.cpp
    src/core/Pipeline.h src/core/Pipeline.cpp
    src/core/ReferenceCard.h src/core/ReferenceCard.cpp
    src/core/Exif.h src/core/Exif.cpp
    src/core/CvQt.h src/core/CvQt.cpp
    src/core/ImageLoader.h src/core/ImageLoader.cpp
    src/core/Orientation.h src/core/Orientation.cpp
    src/core/Ocr.h src/core/Ocr.cpp
    src/core/Tessdata.h src/core/Tessdata.cpp
    src/core/Models.cpp
    src/core/Scan.h src/core/Scan.cpp
)
target_include_directories(deltos_core PUBLIC src)
target_link_libraries(deltos_core PUBLIC ${OpenCV_LIBS} PkgConfig::Tesseract Qt6::Gui)
if(HEIF_FOUND)
    target_link_libraries(deltos_core PUBLIC PkgConfig::HEIF)
    target_compile_definitions(deltos_core PRIVATE DELTOS_HAVE_HEIF)
endif()

# GUI application
set(DELTOS_SOURCES
    src/main.cpp
    src/cli/Cli.h src/cli/Cli.cpp
    src/ui/MainWindow.h src/ui/MainWindow.cpp
    src/ui/QuadEditorView.h src/ui/QuadEditorView.cpp
    src/ui/BusyOverlay.h src/ui/BusyOverlay.cpp
    src/ui/PageModel.h src/ui/PageModel.cpp
    src/ui/OcrTextItem.h src/ui/OcrTextItem.cpp
    src/ui/LanguageDialog.h src/ui/LanguageDialog.cpp
    src/ui/QrImage.h src/ui/QrImage.cpp
    src/export/PdfExporter.h src/export/PdfExporter.cpp
    src/export/ImageExporter.h src/export/ImageExporter.cpp
    src/net/LocalSendReceiver.h src/net/LocalSendReceiver.cpp
    third_party/qrcodegen/qrcodegen.hpp third_party/qrcodegen/qrcodegen.cpp
    icons/deltos.qrc
)
add_executable(deltos ${DELTOS_SOURCES})
target_include_directories(deltos PRIVATE third_party/qrcodegen)
target_link_libraries(deltos PRIVATE deltos_core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::Network)

set(DELTOS_MODEL ${CMAKE_SOURCE_DIR}/models/fastvit_sa24_h_e_bifpn_256_fp32.onnx)

# Application icon and model per platform
if(APPLE)
    set_source_files_properties(icons/deltos.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(${DELTOS_MODEL} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/models)
    target_sources(deltos PRIVATE icons/deltos.icns ${DELTOS_MODEL})
    set_target_properties(deltos PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_BUNDLE_NAME Deltos
        MACOSX_BUNDLE_GUI_IDENTIFIER onl.ycode.Deltos
        MACOSX_BUNDLE_ICON_FILE deltos.icns
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
elseif(WIN32)
    target_sources(deltos PRIVATE icons/deltos.rc)
    set_target_properties(deltos PROPERTIES WIN32_EXECUTABLE ON)
    # The same program again, as a console application. A Windows GUI binary
    # detaches from the shell that started it: the prompt comes back at once and
    # whatever it prints goes nowhere, which makes --no-gui useless. deltos-cli
    # is what a script or a command prompt should call.
    add_executable(deltos-cli ${DELTOS_SOURCES} icons/deltos.rc)
    target_include_directories(deltos-cli PRIVATE third_party/qrcodegen)
    target_link_libraries(deltos-cli PRIVATE deltos_core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::Network)
else()
    include(GNUInstallDirs)
    install(TARGETS deltos RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${DELTOS_MODEL} DESTINATION ${CMAKE_INSTALL_DATADIR}/deltos/models)
    # Desktop entry, icons and metadata are named after the application id, which
    # is what Flatpak requires and what the desktop shells match a window to.
    install(FILES icons/onl.ycode.Deltos.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES icons/onl.ycode.Deltos.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES docs/deltos.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
    install(FILES icons/onl.ycode.Deltos.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    foreach(s 16 32 48 64 128 256 512)
        install(FILES icons/deltos-${s}.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${s}x${s}/apps RENAME onl.ycode.Deltos.png)
    endforeach()
endif()

# Model next to the binary for running from the build tree
file(COPY ${DELTOS_MODEL} DESTINATION ${CMAKE_BINARY_DIR}/models)
