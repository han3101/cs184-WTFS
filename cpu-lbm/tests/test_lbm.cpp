#include "../src/field.h"
#include "../src/lbm.h"
#include "../src/boundary.h"
#include <cstdio>
#include <cmath>

static int failures=0;
static void check(bool c, const char* n){ if(c) std::printf("PASS %s\n",n); else{ std::printf("FAIL %s\n",n); failures++; } }

int main(){
  using namespace cpu_lbm;
  // equilibrium mass/momentum consistency at uniform state
  {
    Field2D f(4,4);
    f.initEquilibrium(0.05f, 0.02f, 1.0f);
    // Each cell's sum_q feq should be rho=1 and momentum = rho*u
    bool ok=true;
    for(size_t i=0;i<f.ncells();++i){
      float sum=0, mx=0, my=0;
      for(int q=0;q<d2q9::Q;++q){ float fq=f.f[f.fIdx(q,i)]; sum+=fq; mx+=fq*d2q9::cx[q]; my+=fq*d2q9::cy[q]; }
      if(std::abs(sum-1.0f)>1e-5f || std::abs(mx-0.05f)>1e-5f || std::abs(my-0.02f)>1e-5f) ok=false;
    }
    check(ok, "feq mass/momentum");
  }
  // collide conserves mass/ momentum at uniform equilibrium (f=feq -> f_star = f)
  {
    Field2D fld(8,8);
    fld.initEquilibrium(0.05f, 0.0f, 1.0f);
    fld.macroscopic();
    float tau=0.6f;
    // save mass before
    double massBefore=0; for(auto v:fld.f) massBefore+=v;
    collideBGK(fld, tau);
    // after collide, mass per cell preserved (since f=feq, f_star=f)
    double massAfter=0; for(auto v:fld.f_next) massAfter+=v;
    check(std::abs(massBefore-massAfter)<1e-4, "collide conserves mass at equilibrium");
  }
  // uniform flow streaming without obstacles/boundaries should leave field unchanged after collide+stream+macroscopic
  {
    Field2D fld(16,16);
    fld.initEquilibrium(0.05f, 0.0f, 1.0f);
    fld.macroscopic();
    collideAndStream(fld, 0.6f);
    // ignore boundary rows/cols where streaming hits domain edge; check interior
    bool ok=true;
    for(int y=2;y<fld.ny-2;++y) for(int x=2;x<fld.nx-2;++x){
      size_t idx=fld.idx(x,y);
      float rho=fld.rho[idx]; // rho hasn't been recomputed after stream; check f distribution symmetry instead
      (void)rho;
    }
    fld.macroscopic();
    for(int y=2;y<fld.ny-2;++y) for(int x=2;x<fld.nx-2;++x){
      size_t idx=fld.idx(x,y);
      if(std::abs(fld.ux[idx]-0.05f)>1e-5f || std::abs(fld.uy[idx])>1e-5f) ok=false;
    }
    check(ok, "uniform streaming preserves u interior");
  }
  // Closed box: every boundary is a bounce-back wall (no inlet/outlet applied).
  // Mass must be conserved exactly and the initial uniform momentum must decay
  // to rest. A bounce-back that injects or loses populations fails both.
  {
    Field2D fld(24,24);
    fld.initEquilibrium(0.05f, 0.0f, 1.0f);
    fld.macroscopic();
    double mass0=0; for(auto v:fld.f) mass0+=v;
    for(int s=0;s<400;++s){ collideAndStream(fld, 0.8f); fld.macroscopic(); }
    double mass1=0, mom=0, umax=0;
    for(auto v:fld.f) mass1+=v;
    for(size_t i=0;i<fld.ncells();++i){
      mom += fld.rho[i]*fld.ux[i];
      umax = std::max(umax, double(std::abs(fld.ux[i])));
    }
    check(std::abs(mass1-mass0)/mass0 < 1e-5, "closed box conserves mass");
    check(std::isfinite(umax) && umax < 0.005, "closed box decays to rest (no-slip walls)");
    check(std::abs(mom) < 1e-3, "closed box sheds x-momentum into walls");
  }
  // Gate A1 — Poiseuille. Velocity inlet + zero-gradient outlet + bounce-back
  // side walls must relax to the analytic parabolic profile downstream of the
  // entrance length. This is the cheapest test that exercises walls, inlet,
  // outlet, and the unit system simultaneously.
  {
    const int nx=96, ny=21;
    const float u0=0.05f, tau=0.8f;   // nu = (tau-0.5)/3 = 0.1
    Field2D fld(nx,ny);
    fld.initEquilibrium(u0, 0.0f, 1.0f);
    fld.macroscopic();
    for(int s=0;s<12000;++s){
      collideAndStream(fld, tau);
      boundary::applyAll(fld, u0);
      fld.macroscopic();
    }
    // Sample a column well past the entrance length, short of the outlet.
    const int xs = nx - 12;
    // Halfway bounce-back puts the no-slip walls at y=-0.5 and y=ny-0.5, so the
    // channel height is exactly ny and the cell centre offset is +0.5.
    const float H = float(ny);
    float umax = 0.0f;
    for(int y=0;y<ny;++y) umax = std::max(umax, fld.ux[fld.idx(xs,y)]);
    double num=0, den=0;
    for(int y=0;y<ny;++y){
      float yc = float(y) + 0.5f;
      float analytic = umax * 4.0f * yc * (H - yc) / (H*H);
      float d = fld.ux[fld.idx(xs,y)] - analytic;
      num += double(d)*d;
      den += double(analytic)*analytic;
    }
    double l2 = std::sqrt(num/den);
    std::printf("  poiseuille: umax=%.5f relative L2=%.2e\n", umax, l2);
    check(l2 < 1e-3, "Poiseuille profile matches analytic (rel L2 < 1e-3)");
  }
  std::printf("%d failures\n", failures);
  return failures==0?0:1;
}
