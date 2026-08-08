#include "../src/field.h"
#include <cstdio>
#include <cmath>
#include <cassert>

// No external test framework in Phase 0a — keeps offline builds green.
// Each check prints PASS/FAIL and returns non-zero on any failure.

static int failures = 0;
static void check(bool cond, const char *name) {
  if (cond) std::printf("PASS %s\n", name);
  else { std::printf("FAIL %s\n", name); failures++; }
}

int main() {
  using namespace cpu_lbm;

  // Field2D sizing and ncells
  {
    Field2D f(256, 128);
    check(f.nx == 256 && f.ny == 128, "size");
    check(f.ncells() == 256u*128u, "ncells");
    check(f.ux.size() == f.ncells() && f.uy.size() == f.ncells(), "SoA size");
  }

  // idx round-trip
  {
    Field2D f(16, 12);
    bool ok = true;
    for (int y = 0; y < f.ny; ++y)
      for (int x = 0; x < f.nx; ++x) {
        size_t i = f.idx(x, y);
        int rx, ry;
        f.idxToXY(i, rx, ry);
        if (rx != x || ry != y) ok = false;
      }
    check(ok, "idx round-trip");
  }

  // idx formula is y*nx + x (documents future f[q*ncells + idx] SoA)
  {
    Field2D f(10, 10);
    check(f.idx(0,0)==0 && f.idx(9,0)==9 && f.idx(0,1)==10 && f.idx(3,7)==73, "idx linear formula");
  }

  // idx_clamped
  {
    Field2D f(8, 8);
    check(f.idx_clamped(-1,-1)==f.idx(0,0), "clamp low");
    check(f.idx_clamped(100,100)==f.idx(7,7), "clamp high");
  }

  // fillUniform + sample at cell centers and mid-cell (bilinear)
  {
    Field2D f(4, 4);
    f.fillUniform(0.05f, -0.02f);
    float ux, uy;
    f.sample(1.0f, 1.0f, ux, uy);
    check(std::abs(ux - 0.05f) < 1e-6f && std::abs(uy + 0.02f) < 1e-6f, "sample uniform integer");
    f.sample(1.5f, 1.5f, ux, uy);
    check(std::abs(ux - 0.05f) < 1e-6f && std::abs(uy + 0.02f) < 1e-6f, "sample uniform half");
  }

  // Bilinear with gradient — simple 2x2 field: ux = x, uy = y
  {
    Field2D f(2, 2);
    for (int y = 0; y < 2; ++y)
      for (int x = 0; x < 2; ++x) {
        f.ux_at(x, y) = float(x);
        f.uy_at(x, y) = float(y);
      }
    float ux, uy;
    f.sample(0.5f, 0.5f, ux, uy);
    check(std::abs(ux - 0.5f) < 1e-6f && std::abs(uy - 0.5f) < 1e-6f, "bilinear gradient center");
    f.sample(0.0f, 0.0f, ux, uy);
    check(std::abs(ux - 0.0f) < 1e-6f && std::abs(uy - 0.0f) < 1e-6f, "bilinear corner");
  }

  // Field3D indexing
  {
    Field3D f(4, 3, 2);
    bool ok = true;
    for (int z = 0; z < f.nz; ++z)
      for (int y = 0; y < f.ny; ++y)
        for (int x = 0; x < f.nx; ++x) {
          size_t i = f.idx(x,y,z);
          int rx, ry, rz;
          f.idxToXYZ(i, rx, ry, rz);
          if (rx!=x || ry!=y || rz!=z) ok = false;
        }
    check(ok, "Field3D idx round-trip");
    check(f.idx(0,0,0)==0 && f.idx(1,0,0)==1 && f.idx(0,1,0)==4 && f.idx(0,0,1)==12, "Field3D linear formula");
  }

  std::printf("%d failures\n", failures);
  return failures == 0 ? 0 : 1;
}
