#include "../src/field.h"
#include "../src/tracers.h"
#include <cstdio>
#include <cmath>

static int failures = 0;
static void check(bool cond, const char *name) {
  if (cond) std::printf("PASS %s\n", name);
  else { std::printf("FAIL %s\n", name); failures++; }
}

int main() {
  using namespace cpu_lbm;

  // Init count stable
  {
    Field2D f(64, 32);
    f.fillUniform(0.05f, 0.0f);
    TracerSet ts(1000, 64, 32, 42);
    check(ts.size() == 1000, "init count");
  }

  // Uniform flow: particle at inlet advances ~u*dt per step, recycles at outlet
  {
    const int nx = 64, ny = 16;
    Field2D f(nx, ny);
    f.fillUniform(0.05f, 0.0f);
    TracerSet ts;
    ts.init(1, nx, ny, 123);
    // Place one tracer at (0, 8) deterministically
    ts.data()[0].x = 0.0f;
    ts.data()[0].y = 8.0f;
    const float dt = 1.0f;
    const float u0 = 0.05f;
    int steps = int(std::ceil(float(nx) / (u0 * dt))) + 5;
    int totalRecycled = 0;
    for (int i = 0; i < steps; ++i) totalRecycled += ts.advect(f, dt);
    check(totalRecycled >= 1, "recycle after nx/u steps");
    check(ts.size() == 1, "count stable after recycle");
    // After recycle, x should be near inlet (0..1)
    check(ts.data()[0].x >= 0.0f && ts.data()[0].x < 2.0f, "recycled x near inlet");
  }

  // Add/remove decouples
  {
    TracerSet ts(100, 32, 32, 7);
    ts.add(50);
    check(ts.size() == 150, "add");
    ts.remove(20);
    check(ts.size() == 130, "remove");
    ts.remove(1000);
    check(ts.size() == 0, "remove all");
  }

  // Advect with zero velocity — positions unchanged
  {
    Field2D f(32, 32);
    f.fillUniform(0.0f, 0.0f);
    TracerSet ts(10, 32, 32, 99);
    auto before = ts.data();
    ts.advect(f, 1.0f);
    bool same = true;
    for (size_t i = 0; i < ts.size(); ++i)
      if (std::abs(ts.data()[i].x - before[i].x) > 1e-6f || std::abs(ts.data()[i].y - before[i].y) > 1e-6f) same = false;
    check(same, "zero field no motion");
  }

  // Spanwise wrapping
  {
    Field2D f(32, 32);
    f.fillUniform(0.0f, 0.1f); // upward drift
    TracerSet ts(1, 32, 32, 1);
    ts.data()[0].x = 16; ts.data()[0].y = 31.9f;
    ts.advect(f, 2.0f); // y would exceed 32
    check(ts.data()[0].y >= 0.0f && ts.data()[0].y < 32.0f, "y wrap");
  }

  // Stats monotonic (smoke)
  {
    Field2D f(128, 64);
    f.fillUniform(0.05f, 0.0f);
    TracerSet ts(5000, 128, 64, 42);
    auto s0 = ts.stats();
    for (int i = 0; i < 100; ++i) ts.advect(f, 1.0f);
    auto s1 = ts.stats();
    // avg_x may wrap due to recycling, but total recycled should be >0
    check(s1.recycled > s0.recycled, "stats recycled monotonic");
  }

  std::printf("%d failures\n", failures);
  return failures == 0 ? 0 : 1;
}
