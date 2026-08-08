#pragma once
#include "field.h"
#include "obstacle.h"
#include <vector>
#include <random>
#include <cstdint>
#include <algorithm>

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
// Supports both continuous stream recycling and periodic pulsed wave emission
// (e.g. 300 particles every 0.3s) with live adjustable frequency and batch size.
class TracerSet {
public:
  TracerSet() = default;
  TracerSet(int n, int nx, int ny, uint32_t seed = 42) { init(n, nx, ny, seed); }

  void init(int n, int nx_, int ny_, uint32_t seed = 42);
  void reseed(uint32_t seed);
  void clear();

  // Periodic pulsed emission configuration (default: 300 particles every 0.3s)
  void setEmitConfig(int count, float intervalSec, bool periodic = true);
  int emitCount() const { return emitCount_; }
  float emitInterval() const { return emitInterval_; }
  bool periodicEmission() const { return periodicEmission_; }

  void setEmitCount(int count) { emitCount_ = std::max(1, count); }
  void setEmitInterval(float intervalSec) { emitInterval_ = std::clamp(intervalSec, 0.02f, 10.0f); }
  void setPeriodicEmission(bool enable) { periodicEmission_ = enable; }
  void togglePeriodicEmission() { periodicEmission_ = !periodicEmission_; }

  int maxTracers() const { return maxTracers_; }
  void setMaxTracers(int maxCount) { maxTracers_ = std::max(100, maxCount); }

  // Step emission timer by realDt (seconds), emits emitCount_ particles whenever timer fires.
  // Returns number of new particles emitted.
  int stepEmission(float dtSec);

  // Emit an immediate burst of count particles at the inlet [0, 1) x [0, ny)
  int emitBurst(int count);

  // Advect all tracers by dt using velocity sampled from field.
  // Recycles or removes exiting tracers according to periodicEmission mode.
  int advect(const Field2D &field, float dt);
  int advect(const Field2D &field, float dt,
             const std::vector<Obstacle>& obstacles, CollMode mode);

  // Add/remove tracers manually
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

  int emitCount_ = 300;           // default: 300 particles per iteration
  float emitInterval_ = 0.30f;     // default: every 0.3 seconds
  float emitTimer_ = 0.0f;
  bool periodicEmission_ = false;  // false for classic headless/test, enabled in live viewer
  int maxTracers_ = 50000;

  void recycleOne(Tracer &t);
};

} // namespace cpu_lbm
