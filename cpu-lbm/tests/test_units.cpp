#include "../src/units.h"
#include <cstdio>
#include <cmath>

static int failures=0;
static void check(bool c, const char* n){ if(c) std::printf("PASS %s\n",n); else{ std::printf("FAIL %s\n",n); failures++; } }

int main(){
  using namespace cpu_lbm;
  // Default
  check(U_LB_DEFAULT==0.05f, "U_LB_DEFAULT 0.05");
  // fromRe Re=100 D=20 -> nu=0.01 tau=0.53
  {
    auto u = LbUnits::fromRe(100, 20, 0.05f);
    check(std::abs(u.nu_lb - 0.01f)<1e-6f, "nu_lb Re100 D20");
    check(std::abs(u.tau - 0.53f)<1e-6f, "tau Re100 D20");
    check(u.stable(), "stable Re100");
    check(u.lowMach(), "lowMach 0.05");
  }
  // Re=1000 D=20 -> nu=0.001 tau=0.503 unstable
  {
    auto u = LbUnits::fromRe(1000, 20, 0.05f);
    check(!u.stable(), "unstable Re1000 D20");
  }
  // tauFromRe helper
  {
    float t = tauFromRe(100, 20, 0.05f);
    check(std::abs(t-0.53f)<1e-6f, "tauFromRe");
  }
  // dx/dt with physical scales
  {
    auto u = LbUnits::fromRe(100, 20, 0.05f, 0.1f, 1.0f); // L_phys=0.1m, u_phys=1m/s
    check(std::abs(u.dx - 0.005f)<1e-7f, "dx");
    check(std::abs(u.dt - 0.00025f)<1e-7f, "dt");
  }
  // Round-trip: physical length = lattice * dx
  {
    auto u = LbUnits::fromRe(150, 30, 0.05f, 0.3f, 2.0f);
    float L_lat = 30; float L_phys = L_lat * u.dx;
    check(std::abs(L_phys - 0.3f)<1e-6f, "round-trip L");
  }
  std::printf("%d failures\n", failures);
  return failures==0?0:1;
}
