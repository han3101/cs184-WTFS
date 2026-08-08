#pragma once
#include <cstdint>

namespace cpu_lbm {

// Phase 0a stub — just enough to give Field and Tracers a lattice velocity scale.
// Full physical ↔ lattice conversion (Re, tau, dx, dt) lands in Phase 0c (units.h).
// Keeping this header stable means Phase 0a code doesn't need to change later.

constexpr float U_LB_DEFAULT = 0.05f;   // Mach ~0.087, well below cs~0.577
constexpr float CS2 = 1.0f / 3.0f;
constexpr float CS = 0.57735026919f;    // sqrt(1/3)

// Domain defaults for Phase 0a headless run
constexpr int NX_DEFAULT = 256;
constexpr int NY_DEFAULT = 128;

} // namespace cpu_lbm
