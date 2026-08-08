#pragma once
#include "field.h"
#include "obstacle.h"
#include <vector>
#include <random>
#include <cstdint>

namespace cpu_lbm {

struct Tracer {
  float x = 0.0f; // continuous grid coordinates in [0, nx) x [0, ny)
  float y = 0.0f;
};

struct TracerStats {
  uint64_t recycled = 0;
  float avg_x = 0.0f;
  float avg_y = 0.0f;
};

// Massless tracers advected by Field2D.
// Count is a pure rendering knob — never feeds back into the field.
// Sampling uses Field2D::sample (bilinear) so tracers see a smooth field.
class TracerSet {
public:
  TracerSet() = default;
  TracerSet(int n, int nx, int ny, uint32_t seed = 42) { init(n, nx, ny, seed); }

  void init(int n, int nx_, int ny_, uint32_t seed = 42);
  void reseed(uint32_t seed);

  // Advect all tracers by dt using velocity sampled from field.
  // Recycles any tracer that exits the right boundary (x >= nx) back to inlet (x ~ 0).
  // Returns number recycled this step.
  int advect(const Field2D &field, float dt);
  // Advect with obstacle collision. Obstacles in lattice coords. Mode controls response.
  // - Passive: push-out only (future LBM guard, 0c)
  // - Slip: cancel inward normal, keep tangent (0b wind visual, default)
  // - Stick: zero velocity at wall
  // - Bounce: elastic reflection v' = v -2(v·n)n
  int advect(const Field2D &field, float dt,
             const std::vector<Obstacle>& obstacles, CollMode mode);

  // Add/remove tracers (proves count is decoupled)
  void add(int n);
  void remove(int n);

  size_t size() const { return tracers.size(); }
  const std::vector<Tracer>& data() const { return tracers; }
  std::vector<Tracer>& data() { return tracers; }

  TracerStats stats() const;

  int nx = 0, ny = 0;

private:
  std::vector<Tracer> tracers;
  std::mt19937 rng;
  uint64_t totalRecycled = 0;
  void recycleOne(Tracer &t);
};

} // namespace cpu_lbm
