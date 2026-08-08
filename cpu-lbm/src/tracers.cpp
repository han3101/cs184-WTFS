#include "tracers.h"
#include <algorithm>

namespace cpu_lbm {

void TracerSet::init(int n, int nx_, int ny_, uint32_t seed) {
  nx = nx_; ny = ny_;
  rng.seed(seed);
  totalRecycled = 0;
  tracers.resize(size_t(n));
  // Start all particles at the inlet (left edge) so wind is visibly
  // entering from the left rather than being pre-filled across the domain.
  // x in [0,1) gives a thin vertical sheet; y uniform across height.
  // Recycle uses the same inlet range, so steady-state becomes uniform
  // only through obstacle-induced spreading and staggered recycling.
  std::uniform_real_distribution<float> distX(0.0f, 1.0f);
  std::uniform_real_distribution<float> distY(0.0f, float(ny));
  for (auto &t : tracers) {
    t.x = distX(rng);
    t.y = distY(rng);
  }
}

void TracerSet::reseed(uint32_t seed) {
  rng.seed(seed);
  totalRecycled = 0;
  std::uniform_real_distribution<float> distX(0.0f, 1.0f);
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
  return advect(field, dt, {}, CollMode::Passive);
}

int TracerSet::advect(const Field2D &field, float dt,
                      const std::vector<Obstacle>& obstacles, CollMode mode) {
  int recycled = 0;
  const float eps = 1e-3f;
  for (auto &t : tracers) {
    float u, v;
    field.sample(t.x, t.y, u, v);
    float nx_ = t.x + u * dt;
    float ny_ = t.y + v * dt;

    // Obstacle collision — SDF push + mode-dependent velocity response
    if (!obstacles.empty()) {
      // Iterate a few times in case push from one obstacle puts inside another
      for (int iter = 0; iter < 3; ++iter) {
        bool hit = false;
        for (auto &obs : obstacles) {
          float s = sdf(obs, nx_, ny_);
          if (s < 0.0f) {
            hit = true;
            float n_x, n_y;
            sdfNormal(obs, nx_, ny_, n_x, n_y);
            float push = -s + eps;
            nx_ += n_x * push;
            ny_ += n_y * push;

            if (mode == CollMode::Slip) {
              float dot = u * n_x + v * n_y;
              if (dot < 0.0f) {
                // remove inward normal, keep tangent for subsequent checks / next frame
                u -= dot * n_x;
                v -= dot * n_y;
              }
            } else if (mode == CollMode::Stick) {
              // no-slip visual: kill all velocity at wall for this step
              nx_ -= u * dt * 0.5f;
              ny_ -= v * dt * 0.5f;
              // nudge again to stay outside after revert
              nx_ += n_x * eps;
              ny_ += n_y * eps;
              u = 0.0f; v = 0.0f;
            } else if (mode == CollMode::Bounce) {
              float dot = u * n_x + v * n_y;
              float ur = u - 2.0f * dot * n_x;
              float vr = v - 2.0f * dot * n_y;
              u = ur; v = vr;
              // separate a bit along reflected direction to avoid re-hit
              nx_ += u * dt * 0.1f;
              ny_ += v * dt * 0.1f;
            } else {
              // Passive: push only, velocity unchanged
            }
            break; // re-check all obstacles from start
          }
        }
        if (!hit) break;
      }
    }

    t.x = nx_;
    t.y = ny_;

    // Handle spanwise bounds — wrap keeps tracers in domain for uniform flow
    if (t.y < 0.0f) t.y += float(ny);
    if (t.y >= float(ny)) t.y -= float(ny);

    // Left boundary
    if (t.x < 0.0f) t.x += float(nx);

    // Right outlet → recycle to inlet
    if (t.x >= float(nx)) {
      recycleOne(t);
      recycled++;
    } else if (!obstacles.empty()) {
      // Safety: if still inside after push (high dt tunneling), recycle to inlet
      // to avoid particles trapped inside big block
      for (auto &obs : obstacles) {
        if (isInside(obs, t.x, t.y)) { recycleOne(t); recycled++; break; }
      }
    }
  }
  return recycled;
}

void TracerSet::add(int n) {
  if (n <= 0) return;
  // New particles enter at the inlet as well, consistent with left-to-right wind
  std::uniform_real_distribution<float> distX(0.0f, 1.0f);
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
