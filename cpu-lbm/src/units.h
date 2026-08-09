#pragma once
#include <cstdint>
#include <cassert>
#include <cmath>
#include "lattice.h"

namespace cpu_lbm {

// Lattice units — physical <-> lattice conversion.
// Reference: plan.md § Unit system, docs/units.md
//   u_lb = 0.05 (Mach ~0.087, must stay << cs~0.577)
//   nu_lb = u_lb * L_lb / Re
//   tau = 3*nu_lb + 0.5, require tau > 0.51 (BGK stability)
//   dx = L_phys / L_lb, dt = dx * u_lb / u_phys

constexpr float U_LB_DEFAULT = 0.05f;   // lattice velocity well below cs

constexpr int NX_DEFAULT = 256;
constexpr int NY_DEFAULT = 128;

// Full conversion helpers — Phase 0c
struct LbUnits {
  float Re = 100.0f;   // Reynolds number
  float L_lb = 20.0f;  // characteristic length in lattice cells (e.g. cylinder diameter)
  float u_lb = U_LB_DEFAULT;
  float nu_lb = 0.0f;  // lattice viscosity
  float tau = 0.0f;    // relaxation time
  float dx = 1.0f;     // physical length per lattice cell (if L_phys known)
  float dt = 1.0f;     // physical time per lattice step

  // Construct from Reynolds number and lattice characteristic length.
  // L_phys and u_phys are optional — if unknown, dx/dt remain 1.
  static LbUnits fromRe(float Re_, float L_lb_, float u_lb_ = U_LB_DEFAULT,
                        float L_phys = -1.0f, float u_phys = -1.0f) {
    assert(Re_ > 0 && L_lb_ > 0 && u_lb_ > 0);
    LbUnits u;
    u.Re = Re_;
    u.L_lb = L_lb_;
    u.u_lb = u_lb_;
    u.nu_lb = u.u_lb * u.L_lb / u.Re;
    u.tau = 3.0f * u.nu_lb + 0.5f;
    // Stability guard — BGK destabilizes as tau -> 0.5
    // Caller should assert tau > 0.51 before running.
    if (L_phys > 0 && u_phys > 0) {
      u.dx = L_phys / u.L_lb;
      u.dt = u.dx * u.u_lb / u_phys;
    }
    return u;
  }

  bool stable() const { return tau > 0.51f; }
  bool lowMach() const { return u_lb < 0.15f; }
};

// Convenience: compute tau directly
inline float tauFromRe(float Re, float L_lb, float u_lb = U_LB_DEFAULT) {
  float nu = u_lb * L_lb / Re;
  return 3.0f * nu + 0.5f;
}

} // namespace cpu_lbm
