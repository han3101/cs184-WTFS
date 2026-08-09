#pragma once
#include "field.h"

namespace cpu_lbm {

// Equilibrium distribution for D2Q9 at (rho, ux, uy)
inline float equilibrium(int q, float rho, float ux, float uy) {
  float cu = float(d2q9::cx[q]) * ux + float(d2q9::cy[q]) * uy;
  float usq = ux*ux + uy*uy;
  return d2q9::w[q] * rho * (1.0f + 3.0f*cu + 4.5f*cu*cu - 1.5f*usq);
}

// One BGK collide + pull-stream step. tau must be > 0.5 (assert); the field must
// have been initialized via initEquilibrium.
//
// Buffer contract — `f` is always the canonical state, there is no ping-pong swap:
//   collideBGK:  reads f,      writes collided into f_next
//   streamPull:  reads f_next, writes streamed  into f
//
// streamPull fuses halfway bounce-back on solid cells and domain walls, so the
// no-slip surfaces are already correct when it returns. boundary::applyAll then
// overwrites the inlet/outlet columns, and macroscopic() derives rho/u from f.
// Per-step order: collideAndStream -> boundary::applyAll -> macroscopic.

void collideBGK(Field2D& fld, float tau);
void streamPull(Field2D& fld);

// Convenience fused step (collide + stream)
void collideAndStream(Field2D& fld, float tau);

} // namespace cpu_lbm
