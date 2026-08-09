#include "tracers.h"
#include <algorithm>

namespace cpu_lbm {

void TracerSet::init(int n, int nx_, int ny_, uint32_t seed) {
  nx = nx_; ny = ny_;
  rng.seed(seed);
  totalRecycled = 0;
  emitTimer_ = 0.0f;
  tracers.resize(size_t(n));
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
  emitTimer_ = 0.0f;
  std::uniform_real_distribution<float> distX(0.0f, 1.0f);
  std::uniform_real_distribution<float> distY(0.0f, float(ny));
  for (auto &t : tracers) {
    t.x = distX(rng);
    t.y = distY(rng);
  }
}

void TracerSet::clear() {
  tracers.clear();
  emitTimer_ = 0.0f;
}

void TracerSet::setEmitConfig(int count, float intervalSec, bool periodic) {
  emitCount_ = std::max(1, count);
  emitInterval_ = std::clamp(intervalSec, 0.02f, 10.0f);
  periodicEmission_ = periodic;
}

int TracerSet::emitBurst(int count) {
  if (count <= 0 || ny <= 0) return 0;
  if (int(tracers.size()) + count > maxTracers_) {
    count = std::max(0, maxTracers_ - int(tracers.size()));
    if (count == 0) return 0;
  }
  std::uniform_real_distribution<float> distX(0.0f, 1.0f);
  std::uniform_real_distribution<float> distY(0.0f, float(ny));
  size_t oldSize = tracers.size();
  tracers.resize(oldSize + size_t(count));
  for (size_t i = oldSize; i < tracers.size(); ++i) {
    tracers[i].x = distX(rng);
    tracers[i].y = distY(rng);
  }
  return count;
}

int TracerSet::stepEmission(float dtSec) {
  if (!periodicEmission_ || emitInterval_ <= 0.0f) return 0;
  emitTimer_ += dtSec;
  int emitted = 0;
  while (emitTimer_ >= emitInterval_) {
    emitTimer_ -= emitInterval_;
    emitted += emitBurst(emitCount_);
  }
  return emitted;
}

void TracerSet::recycleOne(Tracer &t) {
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
  size_t writeIdx = 0;

  for (size_t i = 0; i < tracers.size(); ++i) {
    auto t = tracers[i];
    float u, v;
    field.sample(t.x, t.y, u, v);
    float nx_ = t.x + u * dt;
    float ny_ = t.y + v * dt;

    // Obstacle collision — SDF push + mode-dependent velocity response
    if (!obstacles.empty()) {
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
                u -= dot * n_x;
                v -= dot * n_y;
              }
            } else if (mode == CollMode::Stick) {
              nx_ -= u * dt * 0.5f;
              ny_ -= v * dt * 0.5f;
              nx_ += n_x * eps;
              ny_ += n_y * eps;
              u = 0.0f; v = 0.0f;
            } else if (mode == CollMode::Bounce) {
              float dot = u * n_x + v * n_y;
              float ur = u - 2.0f * dot * n_x;
              float vr = v - 2.0f * dot * n_y;
              u = ur; v = vr;
              nx_ += u * dt * 0.1f;
              ny_ += v * dt * 0.1f;
            }
            break;
          }
        }
        if (!hit) break;
      }
    }

    t.x = nx_;
    t.y = ny_;

    // Spanwise boundary wrapping
    if (t.y < 0.0f) t.y += float(ny);
    if (t.y >= float(ny)) t.y -= float(ny);
    if (t.x < 0.0f) t.x += float(nx);

    // Outlet check
    if (t.x >= float(nx)) {
      if (periodicEmission_) {
        // In periodic pulsed wave mode: particle exited outlet
        recycled++;
        totalRecycled++;
        continue; // drops exited particle, keeping clean wavefronts
      } else {
        recycleOne(t);
        recycled++;
      }
    } else if (!obstacles.empty()) {
      bool trapped = false;
      for (auto &obs : obstacles) {
        if (isInside(obs, t.x, t.y)) {
          trapped = true;
          break;
        }
      }
      if (trapped) {
        if (periodicEmission_) {
          continue; // drop trapped particle in pulse mode
        } else {
          recycleOne(t);
          recycled++;
        }
      }
    }

    tracers[writeIdx++] = t;
  }

  tracers.resize(writeIdx);
  return recycled;
}

void TracerSet::add(int n) {
  if (n <= 0) return;
  emitBurst(n);
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
