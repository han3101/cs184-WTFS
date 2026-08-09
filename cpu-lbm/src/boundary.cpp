#include "boundary.h"
#include "lattice.h"

namespace cpu_lbm {
namespace boundary {

void buildSolidMask(Field2D& fld, const std::vector<Obstacle>& obstacles) {
  std::fill(fld.solid.begin(), fld.solid.end(), 0);
  if (obstacles.empty()) return;
  for (int y = 0; y < fld.ny; ++y) {
    for (int x = 0; x < fld.nx; ++x) {
      size_t i = fld.idx(x,y);
      // Sample SDF at cell center (x+0.5, y+0.5) for less aliasing than integer corner
      float px = float(x) + 0.5f;
      float py = float(y) + 0.5f;
      for (auto &obs : obstacles) {
        if (sdf(obs, px, py) < 0.0f) { fld.solid[i]=1; break; }
      }
    }
  }
}

void applyZouHeInlet(Field2D& fld, float u0) {
  // Zou-He velocity inlet at x=0, imposing ux=u0, uy=0.
  // After streaming, the populations pointing east are unknown (nothing upstream
  // to stream them in): f1 (E), f5 (NE), f8 (SE). Known: f0, f2 (N), f3 (W),
  // f4 (S), f6 (NW), f7 (SW).
  //   rho = [f0+f2+f4 + 2(f3+f6+f7)] / (1 - ux)
  //   f1 = f3 + 2/3 rho ux
  //   f5 = f7 + 1/2 (f4 - f2) + 1/6 rho ux + 1/2 rho uy
  //   f8 = f6 + 1/2 (f2 - f4) + 1/6 rho ux - 1/2 rho uy
  // The 1/2(f4-f2) transverse term is what actually enforces uy=0; without it
  // the inlet leaks uy = (f2-f4)/rho. With uy=0 the rho uy terms drop out.
  for(int y=0;y<fld.ny;++y){
    size_t i = fld.idx(0,y);
    if(fld.solid[i]) continue;
    float f0=fld.f[fld.fIdx(0,i)], f2=fld.f[fld.fIdx(2,i)], f3=fld.f[fld.fIdx(3,i)];
    float f4=fld.f[fld.fIdx(4,i)], f6=fld.f[fld.fIdx(6,i)], f7=fld.f[fld.fIdx(7,i)];
    float rho = (f0 + f2 + f4 + 2.0f*(f3 + f6 + f7)) / (1.0f - u0);
    fld.f[fld.fIdx(1,i)] = f3 + 2.0f/3.0f * rho * u0;
    fld.f[fld.fIdx(5,i)] = f7 + 0.5f*(f4 - f2) + 1.0f/6.0f * rho * u0;
    fld.f[fld.fIdx(8,i)] = f6 + 0.5f*(f2 - f4) + 1.0f/6.0f * rho * u0;
    // rho/ux/uy are intentionally not written here — macroscopic() derives them
    // from f, which is the only way the imposed velocity is actually observable.
  }
}

void applyOutlet(Field2D& fld) {
  int xo = fld.nx-1;
  int xn = fld.nx-2;
  for(int y=0;y<fld.ny;++y){
    size_t io = fld.idx(xo,y);
    size_t inn = fld.idx(xn,y);
    if(fld.solid[io]||fld.solid[inn]) continue;
    for(int q=0;q<d2q9::Q;++q){
      fld.f[fld.fIdx(q,io)] = fld.f[fld.fIdx(q,inn)];
    }
  }
}

} // namespace boundary
} // namespace cpu_lbm
