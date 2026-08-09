#pragma once
#include <vector>
#include <cassert>
#include <cmath>
#include <algorithm>
#include "lattice.h"

namespace cpu_lbm {

// 2D field with SoA velocity storage + LBM distributions.
// Layout is chosen to make GPU port mechanical:
//   - flat index: idx = y * nx + x  (row-major, x fastest)
//   - velocity SoA: ux[idx], uy[idx]  (derived from distributions)
//   - LBM SoA: f[q*ncells + idx] ping-pong between f and f_next — identical in WGSL
//   - helpers idxToXY, inBounds, clamp are shared by tracers and LBM kernels.
struct Field2D {
  int nx = 0;
  int ny = 0;
  // SoA velocity components in lattice units — derived views computed by macroscopic()
  std::vector<float> ux;
  std::vector<float> uy;
  // Density (derived)
  std::vector<float> rho;
  // LBM distributions SoA: f[q*ncells + idx], Q=9 for D2Q9
  std::vector<float> f;
  std::vector<float> f_next;
  // Solid mask: 1 if cell is inside obstacle
  std::vector<char> solid;

  Field2D() = default;
  Field2D(int nx_, int ny_) { resize(nx_, ny_); }

  void resize(int nx_, int ny_) {
    assert(nx_ > 0 && ny_ > 0);
    nx = nx_;
    ny = ny_;
    size_t n = size_t(nx) * size_t(ny);
    ux.assign(n, 0.0f);
    uy.assign(n, 0.0f);
    rho.assign(n, 1.0f);
    solid.assign(n, 0);
    f.assign(size_t(d2q9::Q) * n, 0.0f);
    f_next.assign(size_t(d2q9::Q) * n, 0.0f);
  }

  size_t ncells() const { return size_t(nx) * size_t(ny); }
  static constexpr int Q = d2q9::Q;

  // Flat distribution index: f[q*ncells + idx]
  inline size_t fIdx(int q, size_t cell) const {
    return size_t(q) * ncells() + cell;
  }
  inline size_t fIdx(int q, int x, int y) const {
    return size_t(q) * ncells() + idx(x,y);
  }

  // Flat index — same formula LBM will use for f[q*ncells + idx].
  inline size_t idx(int x, int y) const {
    assert(x >= 0 && x < nx && y >= 0 && y < ny);
    return size_t(y) * size_t(nx) + size_t(x);
  }

  inline size_t idx_clamped(int x, int y) const {
    x = std::clamp(x, 0, nx - 1);
    y = std::clamp(y, 0, ny - 1);
    return size_t(y) * size_t(nx) + size_t(x);
  }

  inline void idxToXY(size_t i, int &x, int &y) const {
    assert(i < ncells());
    x = int(i % size_t(nx));
    y = int(i / size_t(nx));
  }

  inline bool inBounds(int x, int y) const {
    return x >= 0 && x < nx && y >= 0 && y < ny;
  }

  inline bool inBoundsFloat(float x, float y) const {
    return x >= 0.0f && x < float(nx) && y >= 0.0f && y < float(ny);
  }

  // Direct access (nearest cell)
  inline float &ux_at(int x, int y) { return ux[idx(x, y)]; }
  inline float &uy_at(int x, int y) { return uy[idx(x, y)]; }
  inline float ux_at(int x, int y) const { return ux[idx(x, y)]; }
  inline float uy_at(int x, int y) const { return uy[idx(x, y)]; }

  // Uniform velocity fill — also initializes f to equilibrium at rho=1.
  // Keeps Phase 0a behavior for --uniform fallback.
  void fillUniform(float ux0, float uy0);

  // Initialize all distributions to equilibrium for given uniform velocity and rho0=1.
  void initEquilibrium(float ux0, float uy0, float rho0 = 1.0f);

  // Compute macroscopic rho and u from f (after streaming).
  void macroscopic();

  // Note: f is the canonical state. collideBGK writes collided into f_next and
  // streamPull gathers back into f, so there is no swap step — see lbm.h.

  // Bilinear sample at continuous grid coordinates.
  // x,y in [0, nx) x [0, ny). Clamps to edge.
  inline void sample(float x, float y, float &out_ux, float &out_uy) const {
    // Clamp to valid interpolation range
    x = std::clamp(x, 0.0f, float(nx - 1) - 1e-6f);
    y = std::clamp(y, 0.0f, float(ny - 1) - 1e-6f);

    int x0 = int(std::floor(x));
    int y0 = int(std::floor(y));
    int x1 = std::min(x0 + 1, nx - 1);
    int y1 = std::min(y0 + 1, ny - 1);
    float fx = x - float(x0);
    float fy = y - float(y0);

    // Bilinear weights
    float w00 = (1 - fx) * (1 - fy);
    float w10 = fx * (1 - fy);
    float w01 = (1 - fx) * fy;
    float w11 = fx * fy;

    size_t i00 = idx(x0, y0);
    size_t i10 = idx(x1, y0);
    size_t i01 = idx(x0, y1);
    size_t i11 = idx(x1, y1);

    out_ux = w00 * ux[i00] + w10 * ux[i10] + w01 * ux[i01] + w11 * ux[i11];
    out_uy = w00 * uy[i00] + w10 * uy[i10] + w01 * uy[i01] + w11 * uy[i11];
  }

  // Nearest-neighbor sample (useful for debugging / comparison)
  inline void sampleNearest(float x, float y, float &out_ux, float &out_uy) const {
    int xi = std::clamp(int(std::round(x)), 0, nx - 1);
    int yi = std::clamp(int(std::round(y)), 0, ny - 1);
    size_t i = idx(xi, yi);
    out_ux = ux[i];
    out_uy = uy[i];
  }
};

// 3D stub — not used in Phase 0a but defines the indexing contract for Phase B.
// Kept header-only and minimal so 2D code doesn't pay for it.
struct Field3D {
  int nx = 0, ny = 0, nz = 0;
  std::vector<float> ux, uy, uz;

  Field3D() = default;
  Field3D(int nx_, int ny_, int nz_) { resize(nx_, ny_, nz_); }

  void resize(int nx_, int ny_, int nz_) {
    assert(nx_ > 0 && ny_ > 0 && nz_ > 0);
    nx = nx_; ny = ny_; nz = nz_;
    size_t n = size_t(nx)*size_t(ny)*size_t(nz);
    ux.assign(n, 0.0f);
    uy.assign(n, 0.0f);
    uz.assign(n, 0.0f);
  }

  size_t ncells() const { return size_t(nx)*size_t(ny)*size_t(nz); }

  inline size_t idx(int x, int y, int z) const {
    assert(x>=0 && x<nx && y>=0 && y<ny && z>=0 && z<nz);
    return (size_t(z)*size_t(ny) + size_t(y))*size_t(nx) + size_t(x);
  }

  inline void idxToXYZ(size_t i, int &x, int &y, int &z) const {
    assert(i < ncells());
    x = int(i % size_t(nx));
    size_t t = i / size_t(nx);
    y = int(t % size_t(ny));
    z = int(t / size_t(ny));
  }

  void fillUniform(float ux0, float uy0, float uz0) {
    std::fill(ux.begin(), ux.end(), ux0);
    std::fill(uy.begin(), uy.end(), uy0);
    std::fill(uz.begin(), uz.end(), uz0);
  }
};

} // namespace cpu_lbm
