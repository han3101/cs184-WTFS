#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

namespace cpu_lbm {

enum class CollMode {
  Passive, // push-out only, no velocity change — future LBM guard (0c)
  Slip,    // cancel inward normal, keep tangent — wind slip visual (0b default)
  Stick,   // zero velocity at wall — no-slip visual
  Bounce   // elastic reflection v' = v -2(v·n)n — particle billiard
};

inline const char* collModeName(CollMode m) {
  switch (m) {
    case CollMode::Passive: return "passive";
    case CollMode::Slip: return "slip";
    case CollMode::Stick: return "stick";
    case CollMode::Bounce: return "bounce";
  }
  return "unknown";
}

inline CollMode collModeFromString(const char* s) {
  std::string a(s);
  std::transform(a.begin(), a.end(), a.begin(), ::tolower);
  if (a=="passive"||a=="none") return CollMode::Passive;
  if (a=="slip"||a=="slide") return CollMode::Slip;
  if (a=="stick"||a=="noslip") return CollMode::Stick;
  if (a=="bounce"||a=="elastic"||a=="reflect") return CollMode::Bounce;
  return CollMode::Slip;
}

struct Obstacle {
  enum Type { CIRCLE, AABB } type = CIRCLE;
  // common center
  float cx = 0, cy = 0;
  // circle
  float r = 0;
  // aabb half-extents
  float hx = 0, hy = 0;

  static Obstacle makeCircle(float cx_, float cy_, float r_) {
    Obstacle o; o.type=CIRCLE; o.cx=cx_; o.cy=cy_; o.r=r_; return o;
  }
  static Obstacle makeAABB(float cx_, float cy_, float hx_, float hy_) {
    Obstacle o; o.type=AABB; o.cx=cx_; o.cy=cy_; o.hx=hx_; o.hy=hy_; return o;
  }
  static Obstacle makeBlock(float cx_, float cy_, float w_, float h_) {
    return makeAABB(cx_, cy_, w_*0.5f, h_*0.5f);
  }
};

// Signed distance field: negative inside, zero on surface
inline float sdf(const Obstacle& o, float px, float py) {
  if (o.type==Obstacle::CIRCLE) {
    float dx = px - o.cx;
    float dy = py - o.cy;
    return std::sqrt(dx*dx+dy*dy) - o.r;
  } else {
    float dx = std::fabs(px - o.cx) - o.hx;
    float dy = std::fabs(py - o.cy) - o.hy;
    float outside = std::sqrt(std::max(dx,0.0f)*std::max(dx,0.0f) + std::max(dy,0.0f)*std::max(dy,0.0f));
    float inside = std::min(std::max(dx,dy), 0.0f);
    return outside + inside;
  }
}

// Normal = normalized gradient of sdf, points outward
// Returns true if valid, false if degenerate (center)
inline bool sdfNormal(const Obstacle& o, float px, float py, float &nx, float &ny) {
  if (o.type==Obstacle::CIRCLE) {
    float dx = px - o.cx;
    float dy = py - o.cy;
    float len = std::sqrt(dx*dx+dy*dy);
    if (len < 1e-6f) { nx=1; ny=0; return false; }
    nx = dx/len; ny = dy/len;
    return true;
  } else {
    // For AABB: compute closest point on box
    float closestX = std::clamp(px, o.cx - o.hx, o.cx + o.hx);
    float closestY = std::clamp(py, o.cy - o.hy, o.cy + o.hy);
    float dx = px - closestX;
    float dy = py - closestY;
    float len = std::sqrt(dx*dx+dy*dy);
    if (len > 1e-6f) {
      nx = dx/len; ny = dy/len; return true;
    }
    // Inside: pick face with minimal penetration
    float distX = o.hx - std::fabs(px - o.cx);
    float distY = o.hy - std::fabs(py - o.cy);
    if (distX < distY) {
      nx = (px > o.cx) ? 1.0f : -1.0f; ny = 0;
    } else {
      nx = 0; ny = (py > o.cy) ? 1.0f : -1.0f;
    }
    return true;
  }
}

inline bool isInside(const Obstacle& o, float px, float py) {
  return sdf(o, px, py) < 0.0f;
}

} // namespace cpu_lbm
