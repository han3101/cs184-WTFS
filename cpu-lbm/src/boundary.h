#pragma once
#include "field.h"
#include "obstacle.h"
#include <vector>

namespace cpu_lbm {
namespace boundary {

// Build solid mask from obstacle SDFs: solid[i]=1 if sdf<0.
void buildSolidMask(Field2D& fld, const std::vector<Obstacle>& obstacles);

// Note: no-slip bounce-back on the solid mask and on the north/south domain
// walls is fused into streamPull() (see lbm.h), not applied as a separate pass.
// Only the inlet/outlet columns need fixing up afterwards.

// Zou-He velocity inlet on west boundary (x=0): impose ux=u0, uy=0, rho computed from known f.
// Assumes D2Q9 ordering and west wall is the inlet.
void applyZouHeInlet(Field2D& fld, float u0);

// Zero-gradient outlet on east boundary (x=nx-1): copy distributions from neighbor column.
void applyOutlet(Field2D& fld);

// Combined step for convenience. Call after collideAndStream, before macroscopic.
inline void applyAll(Field2D& fld, float u0) {
  applyZouHeInlet(fld, u0);
  applyOutlet(fld);
}

} // namespace boundary
} // namespace cpu_lbm
