#pragma once
#include "field.h"
#include <string>
#include <vector>

namespace cpu_lbm {
namespace io {

// Write Field2D velocity magnitude to raw float file (nx*ny floats)
bool writeRaw(const std::string& path, const Field2D& fld);

// Write Field2D velocity as CSV (x,y,ux,uy,rho,solid)
bool writeCSV(const std::string& path, const Field2D& fld);

// Append probe history to CSV
struct HistoryRow { int step; float drag, lift, Cd, Cl, St; };
bool writeProbeCSV(const std::string& path, const std::vector<HistoryRow>& rows);

// Optional PNG via stb_image_write — writes |u| colormap if available, else returns false
bool writePNG(const std::string& path, const Field2D& fld);

} // namespace io
} // namespace cpu_lbm
