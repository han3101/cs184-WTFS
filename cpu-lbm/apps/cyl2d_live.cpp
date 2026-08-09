#define GL_SILENCE_DEPRECATION
#include "field.h"
#include "units.h"
#include "lbm.h"
#include "boundary.h"
#include "obstacle.h"
#include "tracers.h"
#include "live_common.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>

static void print_usage(const char *prog) {
  std::fprintf(stderr,
    "Usage: %s [options]\n"
    "Phase 0c / A3 live viewer — 2D Lattice Boltzmann (D2Q9) + tracers + pulsed wave emission.\n"
    "Options (all optional — you can just run and pick in-viewer):\n"
    "  --nx N               grid width  (default 400)\n"
    "  --ny N               grid height (default 80)\n"
    "  --re F               Reynolds number (default 100.0)\n"
    "  --u0 F               inlet velocity (default 0.05)\n"
    "  --batch N / -p N     particles per pulse iteration (default 300)\n"
    "  --interval F / -i F  emission interval in seconds (default 0.30)\n"
    "  --stream             start in continuous stream mode (default is pulsed wave)\n"
    "  --dt F               advection scale per frame (default 1.0)\n"
    "  --scale N            window scale (default 3)  window = nx*scale x ny*scale\n"
    "  --seed N             RNG seed (default 42)\n"
    "  --circle cx cy r     add circle obstacle at start (may repeat)\n"
    "  --block cx cy w h    add AABB block at start (center + full size)\n"
    "  --no-obstacle        start with no obstacle (empty channel)\n"
    "  --coll MODE          collision: slip|stick|bounce|passive (default passive)\n"
    "  --uniform            uniform flow fallback (bypasses LBM solver)\n",
    prog);
}

int main(int argc, char **argv) {
  int nx = 400;
  int ny = 80;
  float Re = 100.0f;
  float u0 = cpu_lbm::U_LB_DEFAULT;
  int batchCount = 300;
  float intervalSec = 0.30f;
  bool pulseMode = true;
  float dt = 1.0f;
  int scale = 3;
  uint32_t seed = 42;
  bool uniform = false;
  std::vector<cpu_lbm::Obstacle> obstacles;
  cpu_lbm::CollMode collMode = cpu_lbm::CollMode::Passive;
  bool noDefaultObstacle = false;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](const char* name, int n){ if (i+n>=argc){std::fprintf(stderr,"missing value for %s\n",name); print_usage(argv[0]); std::exit(1);} };
    if (a=="--nx" && (need("--nx",1),true)) nx = std::atoi(argv[++i]);
    else if (a=="--ny" && (need("--ny",1),true)) ny = std::atoi(argv[++i]);
    else if (a=="--re" && (need("--re",1),true)) Re = float(std::atof(argv[++i]));
    else if (a=="--u0" && (need("--u0",1),true)) u0 = float(std::atof(argv[++i]));
    else if ((a=="--particles"||a=="--batch"||a=="-p") && (need(a.c_str(),1),true)) batchCount = std::atoi(argv[++i]);
    else if ((a=="--interval"||a=="--freq"||a=="-i") && (need(a.c_str(),1),true)) intervalSec = float(std::atof(argv[++i]));
    else if (a=="--pulse") pulseMode = true;
    else if (a=="--stream"||a=="--continuous") pulseMode = false;
    else if (a=="--dt" && (need("--dt",1),true)) dt = float(std::atof(argv[++i]));
    else if (a=="--scale" && (need("--scale",1),true)) scale = std::atoi(argv[++i]);
    else if (a=="--seed" && (need("--seed",1),true)) seed = uint32_t(std::atoi(argv[++i]));
    else if (a=="--uniform") uniform = true;
    else if (a=="--circle" && (need("--circle",3),true)) { float cx=float(std::atof(argv[++i])), cy=float(std::atof(argv[++i])), r=float(std::atof(argv[++i])); obstacles.push_back(cpu_lbm::Obstacle::makeCircle(cx,cy,r)); }
    else if (a=="--block" && (need("--block",4),true)) { float cx=float(std::atof(argv[++i])), cy=float(std::atof(argv[++i])), w=float(std::atof(argv[++i])), h=float(std::atof(argv[++i])); obstacles.push_back(cpu_lbm::Obstacle::makeBlock(cx,cy,w,h)); }
    else if (a=="--no-obstacle"||a=="--no-obstacles") noDefaultObstacle = true;
    else if (a=="--coll" && (need("--coll",1),true)) collMode = cpu_lbm::collModeFromString(argv[++i]);
    else if (a=="--help"||a=="-h") { print_usage(argv[0]); return 0; }
    else { std::fprintf(stderr,"unknown arg: %s\n",a.c_str()); print_usage(argv[0]); return 1; }
  }
  if (nx<=0||ny<=0||batchCount<=0||scale<=0){ std::fprintf(stderr,"invalid parameters\n"); return 1; }
  if (obstacles.empty() && !noDefaultObstacle) {
    float D = float(ny) * 0.25f;
    obstacles.push_back(cpu_lbm::Obstacle::makeCircle(float(nx)*0.30f, float(ny)*0.5f, D*0.5f));
  }

  cpu_lbm::Field2D field(nx, ny);
  field.initEquilibrium(u0, 0.0f, 1.0f);
  float tau = 0.60f;
  auto updatePhysics = [&]() {
    if (uniform) return;
    float D = float(ny) * 0.25f;
    if (!obstacles.empty() && obstacles[0].type == cpu_lbm::Obstacle::CIRCLE) D = obstacles[0].r * 2.0f;
    else if (!obstacles.empty() && obstacles[0].type == cpu_lbm::Obstacle::AABB) D = obstacles[0].hy * 2.0f;
    auto units = cpu_lbm::LbUnits::fromRe(Re, D, u0);
    tau = units.tau;
    if (!units.stable()) std::fprintf(stderr, "[LBM] WARN tau=%.4f <= 0.51 (unstable) for Re=%.1f D=%.1f\n", tau, Re, D);
    cpu_lbm::boundary::buildSolidMask(field, obstacles);
  };
  updatePhysics();

  cpu_lbm::TracerSet tracers(0, nx, ny, seed);
  tracers.setEmitConfig(batchCount, intervalSec, pulseMode);
  tracers.emitBurst(batchCount);

  try {
    std::string title = uniform ? "WTFS — cyl2d_live [UNIFORM]" : "WTFS — cyl2d_live (LBM D2Q9)";
    cpu_lbm::FieldViewer viewer(nx, ny, scale, title);
    cpu_lbm::LiveParams params{batchCount, intervalSec, pulseMode, dt, u0, collMode};

    auto physicsStep = [&](){
      if (uniform) return;
      cpu_lbm::collideAndStream(field, tau);
      cpu_lbm::boundary::applyAll(field, u0);
      field.macroscopic();
    };

    auto titleFn = [&](int fps, float mlups) -> std::string {
      char buf[360];
      if (uniform) {
        std::snprintf(buf, sizeof(buf), "WTFS cyl2d_live [UNIFORM] %dx%d  %d tracers  [%d/%.2fs %s]  %s  obs=%zu  %.2fx  SPD %.2f  %d FPS",
          nx, ny, int(tracers.size()), params.batchCount, params.intervalSec, tracers.periodicEmission()?"PULSE":"STREAM",
          "running", obstacles.size(), viewer.zoom(), params.dt*params.u0, fps);
      } else {
        std::snprintf(buf, sizeof(buf), "WTFS cyl2d_live (LBM D2Q9) %dx%d  Re=%.0f  tau=%.3f  %d tracers  [%d/%.2fs %s]  %.2fx  SPD %.2f  %d FPS (%.1f MLUPS)",
          nx, ny, Re, tau, int(tracers.size()), params.batchCount, params.intervalSec, tracers.periodicEmission()?"PULSE":"STREAM",
          viewer.zoom(), params.dt*params.u0, fps, mlups);
      }
      return std::string(buf);
    };

    return cpu_lbm::runLiveLoop(viewer, field, tracers, obstacles, params, physicsStep, updatePhysics, titleFn);
  } catch (const std::exception &e) {
    std::fprintf(stderr, "viewer error: %s\n", e.what());
    return 1;
  }
}
