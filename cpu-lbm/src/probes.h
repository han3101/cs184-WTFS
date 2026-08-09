#pragma once
#include "field.h"
#include <vector>
#include <cmath>

namespace cpu_lbm {

// Drag/lift via momentum exchange (Ladd) and wake Strouhal via zero-crossings.
// Designed for D2Q9 cylinder case; other cases use same probes.

struct ProbeResult {
  float drag = 0.0f;
  float lift = 0.0f;
  // Cd, Cl nondimensionalized by (0.5 * rho * u0^2 * D)
  float Cd = 0.0f;
  float Cl = 0.0f;
};

// Compute drag/lift by summing momentum exchange over solid boundary.
// Reference velocity u0 and diameter D required for Cd/Cl normalization.
ProbeResult computeDragLift(const Field2D& fld, float u0, float D);

// Wake probe: record uy at a fixed lattice point over time, estimate Strouhal St = f*D / u0
// by counting zero crossings (or FFT); returns St and dominant frequency.
struct WakeProbe {
  int x = 0, y = 0;      // lattice probe location
  float dt = 1.0f;       // lattice steps between consecutive sample() calls
  std::vector<float> history; // uy history
  // sampleEvery must match the caller's sampling cadence — sampling every 20th
  // step with dt=1 would report a frequency 20x too high.
  void init(int x_, int y_, int sampleEvery = 1) {
    x=x_; y=y_; dt=float(sampleEvery); history.clear();
  }
  void sample(const Field2D& fld); // push current uy
  float estimateSt(float u0, float D) const;
  float estimateFrequency() const;
  // Explicit-interval overloads (dt in lattice steps per sample)
  float estimateSt(float u0, float D, float dt_) const;
  float estimateFrequency(float dt_) const;
};

} // namespace cpu_lbm
