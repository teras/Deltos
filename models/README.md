# Model

`fastvit_sa24_h_e_bifpn_256_fp32.onnx` is the DocAligner heatmap-regression model from
https://github.com/DocsaidLab/DocAligner (FastViT-SA24 backbone, BiFPN neck, 256 px input,
SmartDoc IoU 0.9937), redistributed unchanged under the Apache License 2.0 (see `LICENSE` here).

Input `img`: 1x3x256x256 float RGB, /255 (downscale with INTER_AREA first).
Output `heatmap`: 1x4x128x128, one channel per corner (TL, TR, BR, BL).

At run time the model is looked up in `models/` next to the executable, one level up, the
macOS bundle `Resources/`, `share/deltos/` of the install prefix, the per-user application data
directory and the current directory (`src/core/Models.cpp`). Another DocAligner model with the
same interface can be used with `deltos --model FILE`.

Text orientation uses Tesseract OSD (`osd.traineddata` from the system tessdata).
