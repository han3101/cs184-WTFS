#include "../src/lattice.h"
#include <cstdio>
#include <cmath>

static int failures = 0;
static void check(bool cond, const char *name) {
  if (cond) std::printf("PASS %s\n", name);
  else { std::printf("FAIL %s\n", name); failures++; }
}

int main() {
  using namespace cpu_lbm;
  // D2Q9 weights sum to 1
  {
    float s = 0; for (auto v: d2q9::w) s += v;
    check(std::abs(s - 1.0f) < 1e-6f, "d2q9 sum w =1");
  }
  // D2Q9 sum w*c =0
  {
    float sx=0, sy=0;
    for (int i=0;i<d2q9::Q;++i){ sx += d2q9::w[i]*d2q9::cx[i]; sy += d2q9::w[i]*d2q9::cy[i]; }
    check(std::abs(sx)<1e-6f && std::abs(sy)<1e-6f, "d2q9 sum w*c =0");
  }
  // D2Q9 opp involution
  {
    bool ok=true;
    for (int i=0;i<d2q9::Q;++i) if (d2q9::opp[d2q9::opp[i]]!=i) ok=false;
    // check rest self-opposite, axis pairs
    if (d2q9::opp[0]!=0) ok=false;
    if (d2q9::opp[1]!=3 || d2q9::opp[3]!=1) ok=false;
    if (d2q9::opp[5]!=7 || d2q9::opp[7]!=5) ok=false;
    check(ok, "d2q9 opp involution");
  }
  // D3Q19 weights sum to 1
  {
    float s=0; for(auto v: d3q19::w) s+=v;
    check(std::abs(s-1.0f)<1e-6f, "d3q19 sum w =1");
  }
  // D3Q19 sum w*c =0
  {
    float sx=0,sy=0,sz=0;
    for(int i=0;i<d3q19::Q;++i){ sx+=d3q19::w[i]*d3q19::cx[i]; sy+=d3q19::w[i]*d3q19::cy[i]; sz+=d3q19::w[i]*d3q19::cz[i]; }
    check(std::abs(sx)<1e-6f && std::abs(sy)<1e-6f && std::abs(sz)<1e-6f, "d3q19 sum w*c =0");
  }
  // D3Q19 opp involution
  {
    bool ok=true;
    for(int i=0;i<d3q19::Q;++i) if(d3q19::opp[d3q19::opp[i]]!=i) ok=false;
    check(ok, "d3q19 opp involution");
  }
  // cs2
  check(std::abs(CS2 - 1.0f/3.0f)<1e-7f, "cs2=1/3");
  check(std::abs(CS*CS - CS2)<1e-6f, "cs^2=cs2");
  std::printf("%d failures\n", failures);
  return failures==0?0:1;
}
