#pragma once
#include <array>
#include <cassert>

namespace cpu_lbm {

// D2Q9 and D3Q19 velocity sets, weights, opposite indices.
// cs2 = 1/3, cs = sqrt(1/3) ≈ 0.577
// Phase 0c: D2Q9 is exercised; D3Q19 constants defined for header stability (Phase B).

constexpr float CS2 = 1.0f / 3.0f;
constexpr float CS  = 0.57735026919f;

namespace d2q9 {
constexpr int Q = 9;
// cx, cy for each direction. Ordering: 0 rest, 1-4 axis, 5-8 diagonals.
// This ordering matches common LBM literature and keeps opp simple.
constexpr std::array<int, Q> cx = { 0,  1,  0, -1,  0,  1, -1, -1,  1 };
constexpr std::array<int, Q> cy = { 0,  0,  1,  0, -1,  1,  1, -1, -1 };
// Weights
constexpr std::array<float, Q> w = {
  4.0f/9.0f,
  1.0f/9.0f, 1.0f/9.0f, 1.0f/9.0f, 1.0f/9.0f,
  1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f
};
// Opposite direction
constexpr std::array<int, Q> opp = { 0, 3, 4, 1, 2, 7, 8, 5, 6 };
} // namespace d2q9

namespace d3q19 {
constexpr int Q = 19;
constexpr std::array<int, Q> cx = { 0,  1, -1,  0,  0,  0,  0,  1, -1,  1, -1,  1, -1,  1, -1,  0,  0,  0,  0 };
constexpr std::array<int, Q> cy = { 0,  0,  0,  1, -1,  0,  0,  1,  1, -1, -1,  0,  0,  0,  0,  1, -1,  1, -1 };
constexpr std::array<int, Q> cz = { 0,  0,  0,  0,  0,  1, -1,  0,  0,  0,  0,  1,  1, -1, -1,  1,  1, -1, -1 };
constexpr std::array<float, Q> w = {
  1.0f/3.0f,
  1.0f/18.0f, 1.0f/18.0f, 1.0f/18.0f, 1.0f/18.0f, 1.0f/18.0f, 1.0f/18.0f,
  1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f,
  1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f
};
constexpr std::array<int, Q> opp = { 0, 2, 1, 4, 3, 6, 5, 10, 9, 8, 7, 14, 13, 12, 11, 18, 17, 16, 15 };
} // namespace d3q19

} // namespace cpu_lbm
