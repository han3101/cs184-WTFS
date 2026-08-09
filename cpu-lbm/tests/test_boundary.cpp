#include "../src/field.h"
#include "../src/boundary.h"
#include <cstdio>
#include <cmath>

static int failures=0;
static void check(bool c,const char* n){ if(c) std::printf("PASS %s\n",n); else{ std::printf("FAIL %s\n",n); failures++; } }

int main(){
  using namespace cpu_lbm;
  // buildSolidMask single circle
  {
    Field2D fld(20,20);
    auto obs = Obstacle::makeCircle(10,10,5);
    boundary::buildSolidMask(fld, {obs});
    // center should be solid, corner not
    check(fld.solid[fld.idx(10,10)]==1, "mask center solid");
    check(fld.solid[fld.idx(0,0)]==0, "mask corner fluid");
  }
  // Zou-He inlet imposes u0 — verified by recomputing u FROM f, not by reading
  // back a cached value the boundary wrote itself.
  {
    Field2D fld(8,6);
    fld.initEquilibrium(0.05f, 0.0f, 1.0f);
    // Perturb the inlet column so the transverse correction term has work to do:
    // f2 (N) and f4 (S) unequal is exactly what leaks uy if 1/2(f4-f2) is missing.
    for(int y=0;y<fld.ny;++y){
      size_t i=fld.idx(0,y);
      fld.f[fld.fIdx(2,i)] *= 1.10f;
      fld.f[fld.fIdx(4,i)] *= 0.90f;
    }
    boundary::applyZouHeInlet(fld, 0.05f);
    fld.macroscopic();
    bool okU=true, okV=true;
    for(int y=0;y<fld.ny;++y){
      size_t i=fld.idx(0,y);
      if(std::abs(fld.ux[i]-0.05f)>1e-5f) okU=false;
      if(std::abs(fld.uy[i])>1e-5f) okV=false;
    }
    check(okU, "ZouHe imposes ux=u0 (recomputed from f)");
    check(okV, "ZouHe imposes uy=0 under N/S asymmetry");
  }
  // outlet copies penultimate column
  {
    Field2D fld(6,4);
    fld.initEquilibrium(0.05f,0.0f,1.0f);
    fld.f[fld.fIdx(1,fld.idx(5,2))] = 999.0f; // corrupt outlet
    boundary::applyOutlet(fld);
    check(fld.f[fld.fIdx(1,fld.idx(5,2))] == fld.f[fld.fIdx(1,fld.idx(4,2))], "outlet copies");
  }
  std::printf("%d failures\n", failures);
  return failures==0?0:1;
}
