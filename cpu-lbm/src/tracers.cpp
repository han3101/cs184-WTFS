#include "tracers.h"
#include <algorithm>

namespace cpu_lbm {

void TracerSet::init(int n, int nx_, int ny_, uint32_t seed) {
  nx = nx_; ny = ny_;
  rng.seed(seed);
  totalRecycled = 0;
  tracers.resize(size_t(n));
  std::uniform_real_distribution<float> distX(0.0f, float(nx));
  std::uniform_real_distribution<float> distY(0.0f, float(ny));
  for (auto &t : tracers) {
    t.x = distX(rng);
    t.y = distY(rng);
  }
}

void TracerSet::reseed(uint32_t seed) {
  rng.seed(seed);
  totalRecycled = 0;
  std::uniform_real_distribution<float> distX(0.0f, float(nx));
  std::uniform_real_distribution<float> distY(0.0f, float(ny));
  for (auto &t : tracers) {
    t.x = distX(rng);
    t.y = distY(rng);
  }
}

void TracerSet::recycleOne(Tracer &t) {
  // Place at inlet with a small random offset so particles don't start on a perfect line
  std::uniform_real_distribution<float> distX(0.0f, 1.0f);
  std::uniform_real_distribution<float> distY(0.0f, float(ny));
  t.x = distX(rng);
  t.y = distY(rng);
  totalRecycled++;
}

int TracerSet::advect(const Field2D &field, float dt) {
  int recycled = 0;
  for (auto &t : tracers) {
    float u, v;
    field.sample(t.x, t.y, u, v);
    t.x += u * dt;
    t.y += v * dt;

    // Handle spanwise bounds — clamp / wrap keeps tracers in domain for uniform flow
    // (with LBM and obstacles later, wall bounce-back will be handled by the field)
    if (t.y < 0.0f) t.y += float(ny);
    if (t.y >= float(ny)) t.y -= float(ny);

    // Left boundary (should rarely happen with positive u, but handle anyway)
    if (t.x < 0.0f) t.x += float(nx);

    // Right outlet → recycle to inlet
    if (t.x >= float(nx)) {
      recycleOne(t);
      recycled++;
    }
  }
  return recycled;
}

void TracerSet::add(int n) {
  if (n <= 0) return;
  std::uniform_real_distribution<float> distX(0.0f, float(nx));
  std::uniform_real_distribution<float> distY(0.0f, float(ny));
  size_t old = tracers.size();
  tracers.resize(old + size_t(n));
  for (size_t i = old; i < tracers.size(); ++i) {
    tracers[i].x = distX(rng);
    tracers[i].y = distY(rng);
  }
}

void TracerSet::remove(int n) {
  if (n <= 0 || tracers.empty()) return;
  n = std::min<int>(n, int(tracers.size()));
  tracers.resize(tracers.size() - size_t(n));
}

TracerStats TracerSet::stats() const {
  TracerStats s;
  s.recycled = totalRecycled;
  if (tracers.empty()) return s;
  double sx = 0, sy = 0;
  for (auto &t : tracers) { sx += t.x; sy += t.y; }
  s.avg_x = float(sx / double(tracers.size()));
  s.avg_y = float(sy / double(tracers.size()));
  return s;
}

} // namespace cpu_lbm
