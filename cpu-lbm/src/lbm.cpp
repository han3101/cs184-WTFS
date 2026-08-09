#include "lbm.h"
#include "lattice.h"
#include <cassert>

namespace cpu_lbm {

void collideBGK(Field2D& fld, float tau) {
  assert(tau > 0.5f);
  float invTau = 1.0f / tau;
  size_t n = fld.ncells();
  for (size_t i = 0; i < n; ++i) {
    if (fld.solid[i]) continue;  // solid cells hold no fluid; nothing to relax
    float rho = fld.rho[i];
    float ux = fld.ux[i];
    float uy = fld.uy[i];
    for (int q = 0; q < d2q9::Q; ++q) {
      float feq = equilibrium(q, rho, ux, uy);
      float fq = fld.f[fld.fIdx(q, i)];
      fld.f_next[fld.fIdx(q, i)] = fq - invTau * (fq - feq);
    }
  }
}

void streamPull(Field2D& fld) {
  int nx = fld.nx, ny = fld.ny;
  // Pull (gather) streaming with fused halfway bounce-back:
  //   fluid source  -> f[q][idx] = f_collided[q][idx - c_q]
  //   solid / wall  -> f[q][idx] = f_collided[opp(q)][idx]
  // The second branch is the no-slip wall: the population that would have
  // arrived from the blocked neighbour is the cell's own outgoing population
  // in the opposite direction, reflected. It covers both obstacle surfaces and
  // the north/south domain walls, so no separate bounce-back pass is needed.
  // Inlet (west) and outlet (east) overwrite their columns afterwards.
  //
  // Buffer contract: collideBGK writes collided into f_next; streamPull reads
  // f_next and writes streamed into f. `f` is always the canonical state — no
  // swap. Single branch per direction, which ports directly to one WGSL
  // invocation per cell.
  for (int y = 0; y < ny; ++y) {
    for (int x = 0; x < nx; ++x) {
      size_t idx = fld.idx(x,y);
      if (fld.solid[idx]) continue;  // solid cells carry no fluid state
      for (int q = 0; q < d2q9::Q; ++q) {
        int sx = x - d2q9::cx[q];
        int sy = y - d2q9::cy[q];
        bool inside = (sx >= 0 && sx < nx && sy >= 0 && sy < ny);
        if (inside && !fld.solid[fld.idx(sx, sy)]) {
          fld.f[fld.fIdx(q, idx)] = fld.f_next[fld.fIdx(q, fld.idx(sx, sy))];
        } else {
          fld.f[fld.fIdx(q, idx)] = fld.f_next[fld.fIdx(d2q9::opp[q], idx)];
        }
      }
    }
  }
}

void collideAndStream(Field2D& fld, float tau) {
  collideBGK(fld, tau);
  streamPull(fld);
}

} // namespace cpu_lbm
