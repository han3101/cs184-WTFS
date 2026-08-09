#include "field.h"
#include "units.h"
#include "lbm.h"
#include "boundary.h"
#include "probes.h"
#include "io.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cmath>
#include <vector>

static void print_usage(const char* prog){
  std::fprintf(stderr,
    "Usage: %s [options]\n"
    " Cylinder 2D LBM batch — headless, validates A3 (St, Cd)\n"
    " Options:\n"
    "  --nx N         grid width (default 400)\n"
    "  --ny N         grid height (default 80)\n"
    "  --re F         Reynolds number (default 100)\n"
    "  --u0 F         inlet velocity (default 0.05)\n"
    "  --steps N      steps (default 20000)\n"
    "  --csv PATH     probe history CSV\n"
    "  --raw PATH     raw field dump\n"
    "  --uniform      skip LBM, use uniform field (regression)\n"
    "  --strict       exit nonzero if the A3 gate (St, Cd) is not met\n",
    prog);
}

// Phase A3 acceptance window (plan.md § Verification)
static constexpr float ST_MIN = 0.164f, ST_MAX = 0.167f;
static constexpr float CD_MIN = 1.32f,  CD_MAX = 1.36f;
static constexpr int PROBE_EVERY = 20;  // steps between wake samples

int main(int argc, char** argv){
  int nx=400, ny=80;
  float Re=100.0f;
  float u0=0.05f;
  int steps=20000;
  std::string csvPath, rawPath;
  bool uniform=false, strict=false;
  for(int i=1;i<argc;++i){
    std::string a=argv[i];
    auto need=[&](const char* n){ if(i+1>=argc){ std::fprintf(stderr,"missing %s\n",n); print_usage(argv[0]); std::exit(1);} };
    if(a=="--nx" && (need("--nx"),true)) nx=std::atoi(argv[++i]);
    else if(a=="--ny" && (need("--ny"),true)) ny=std::atoi(argv[++i]);
    else if(a=="--re" && (need("--re"),true)) Re=float(std::atof(argv[++i]));
    else if(a=="--u0" && (need("--u0"),true)) u0=float(std::atof(argv[++i]));
    else if(a=="--steps" && (need("--steps"),true)) steps=std::atoi(argv[++i]);
    else if(a=="--csv" && (need("--csv"),true)) csvPath=argv[++i];
    else if(a=="--raw" && (need("--raw"),true)) rawPath=argv[++i];
    else if(a=="--uniform") uniform=true;
    else if(a=="--strict") strict=true;
    else if(a=="--help"||a=="-h"){ print_usage(argv[0]); return 0; }
    else { std::fprintf(stderr,"unknown %s\n",a.c_str()); print_usage(argv[0]); return 1; }
  }

  // Cylinder centered at 1/4 width
  float D = float(ny) * 0.25f; // diameter ~20 for ny=80
  float cx = float(nx)*0.30f;
  float cy = float(ny)*0.50f;
  float r = D*0.5f;

  cpu_lbm::Field2D fld(nx,ny);
  fld.initEquilibrium(u0, 0.0f, 1.0f);

  // Build mask if not uniform
  std::vector<cpu_lbm::Obstacle> obs;
  auto units = cpu_lbm::LbUnits::fromRe(Re, D, u0);
  if(!uniform){
    obs.push_back(cpu_lbm::Obstacle::makeCircle(cx,cy,r));
    cpu_lbm::boundary::buildSolidMask(fld, obs);
    if(!units.stable()){
      std::fprintf(stderr,"WARN tau=%.4f unstable for Re=%.1f D=%.1f — increase resolution\n", units.tau, Re, D);
    }
  }

  const float tau = units.tau;

  // Wake probe downstream of cylinder, offset off the centreline: uy on the
  // symmetry line is zero until the wake goes unstable, so an on-axis probe
  // only sees shedding once it is fully developed.
  cpu_lbm::WakeProbe wake;
  wake.init(int(cx + D*2.0f), int(cy + D*0.5f), PROBE_EVERY);

  std::vector<cpu_lbm::io::HistoryRow> history;
  history.reserve(steps/50+1);

  for(int step=0; step<steps; ++step){
    if(!uniform){
      cpu_lbm::collideAndStream(fld, tau);
      cpu_lbm::boundary::applyAll(fld, u0);
      fld.macroscopic();
      if(step%PROBE_EVERY==0){
        auto pr = cpu_lbm::computeDragLift(fld, u0, D);
        wake.sample(fld);
        cpu_lbm::io::HistoryRow row;
        row.step=step; row.drag=pr.drag; row.lift=pr.lift; row.Cd=pr.Cd; row.Cl=pr.Cl; row.St=wake.estimateSt(u0,D);
        history.push_back(row);
      }
    } else {
      // uniform regression — keep fillUniform behavior
      if(step==0) fld.fillUniform(u0,0.0f);
    }
    if(step%2000==0 && step>0){
      auto pr = uniform ? cpu_lbm::ProbeResult{} : cpu_lbm::computeDragLift(fld,u0,D);
      std::printf("step %d Cd=%.3f St=%.3f\n", step, pr.Cd, wake.estimateSt(u0,D));
      std::fflush(stdout);
    }
  }

  if(!uniform){
    auto pr = cpu_lbm::computeDragLift(fld, u0, D);
    float St = wake.estimateSt(u0,D);
    std::printf("FINAL Re=%.1f D=%.1f tau=%.4f Cd=%.4f Cl=%.4f St=%.4f\n", Re, D, tau, pr.Cd, pr.Cl, St);
    if(!csvPath.empty()) cpu_lbm::io::writeProbeCSV(csvPath, history);
    if(!rawPath.empty()) cpu_lbm::io::writeRaw(rawPath, fld);

    bool finite = std::isfinite(pr.Cd) && std::isfinite(St);
    bool stOk = finite && St>=ST_MIN && St<=ST_MAX;
    bool cdOk = finite && pr.Cd>=CD_MIN && pr.Cd<=CD_MAX;
    if(!stOk || !cdOk){
      std::fprintf(stderr,
        "gate A3 FAILED: St=%.4f (want [%.3f, %.3f]) Cd=%.4f (want [%.2f, %.2f])%s\n",
        St, ST_MIN, ST_MAX, pr.Cd, CD_MIN, CD_MAX, finite ? "" : " — solution diverged");
      if(strict) return 1;
    } else {
      std::printf("gate A3 PASSED\n");
    }
  } else {
    std::printf("uniform mode done steps=%d\n", steps);
  }
  return 0;
}
