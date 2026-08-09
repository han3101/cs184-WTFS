#include "field.h"
#include "lattice.h"

namespace cpu_lbm {

static inline float feq_q(int q, float rho, float ux, float uy) {
  float cu = float(d2q9::cx[q]) * ux + float(d2q9::cy[q]) * uy;
  float usq = ux*ux + uy*uy;
  return d2q9::w[q] * rho * (1.0f + 3.0f*cu + 4.5f*cu*cu - 1.5f*usq);
}

void Field2D::initEquilibrium(float ux0, float uy0, float rho0) {
  size_t n = ncells();
  for (size_t i = 0; i < n; ++i) {
    ux[i] = ux0;
    uy[i] = uy0;
    rho[i] = rho0;
    for (int q = 0; q < d2q9::Q; ++q) {
      f[fIdx(q, i)] = feq_q(q, rho0, ux0, uy0);
      f_next[fIdx(q, i)] = f[fIdx(q, i)];
    }
  }
}

void Field2D::fillUniform(float ux0, float uy0) {
  initEquilibrium(ux0, uy0, 1.0f);
}

void Field2D::macroscopic() {
  size_t n = ncells();
  for (size_t i = 0; i < n; ++i) {
    if (solid[i]) {
      rho[i] = 1.0f;
      ux[i] = 0.0f;
      uy[i] = 0.0f;
      continue;
    }
    float r = 0.0f, u_x = 0.0f, u_y = 0.0f;
    for (int q = 0; q < d2q9::Q; ++q) {
      float fq = f[fIdx(q, i)];
      r += fq;
      u_x += fq * float(d2q9::cx[q]);
      u_y += fq * float(d2q9::cy[q]);
    }
    rho[i] = r;
    if (r > 1e-12f) {
      ux[i] = u_x / r;
      uy[i] = u_y / r;
    } else {
      ux[i] = 0.0f;
      uy[i] = 0.0f;
    }
  }
}

} // namespace cpu_lbm
