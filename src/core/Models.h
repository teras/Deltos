#pragma once
#include <string>

namespace deltos {

// The DocAligner model shipped with the application (see models/README.md).
inline const char* kModelFile = "fastvit_sa24_h_e_bifpn_256_fp32.onnx";

// Full path of the shipped model, looked up in: models/ next to the executable, one level up,
// the macOS bundle Resources, share/deltos (Linux install), the per-user app data dir, the
// current dir. Empty if not found.
std::string findModel();

} // namespace deltos
