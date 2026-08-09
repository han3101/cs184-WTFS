#define GL_SILENCE_DEPRECATION
#include "field.h"
#include "tracers.h"
#include "units.h"
#include "obstacle.h"
#include "live_common.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static void print_usage(const char *prog) {
  std::fprintf(stderr,
    "Usage: %s [options]\n"
    "Phase 0b live viewer — uniform field + obstacles + tracers + pulsed wave emission.\n"
    "Options (all optional — you can just run and pick in-viewer):\n"
    "  --nx N               grid width  (default %d)\n"
    "  --ny N               grid height (default %d)\n"
    "  --batch N / -p N     particles per pulse iteration (default 300)\n"
    "  --interval F / -i F  emission interval in seconds (default 0.30)\n"
    "  --stream             start in continuous stream mode (default is pulsed wave)\n"
    "  --u0 F               uniform velocity ux (default %.3f)\n"
    "  --dt F               advection scale per frame (default 1.0)\n"
    "  --scale N            window scale (default 3)  window = nx*scale x ny*scale\n"
    "  --seed N             RNG seed (default 42)\n"
    "  --circle cx cy r     add circle obstacle at start (may repeat)\n"
    "  --block cx cy w h    add AABB block at start (center + full size)\n"
    "  --no-obstacle        start with no obstacle (default is big circle)\n"
    "  --coll MODE          collision: slip|stick|bounce|passive (default slip)\n",
    prog, cpu_lbm::NX_DEFAULT, cpu_lbm::NY_DEFAULT, cpu_lbm::U_LB_DEFAULT);
}

int main(int argc, char **argv) {
  int nx = cpu_lbm::NX_DEFAULT;
  int ny = cpu_lbm::NY_DEFAULT;
  int batchCount = 300;
  float intervalSec = 0.30f;
  bool pulseMode = true;
  float u0 = cpu_lbm::U_LB_DEFAULT;
  float dt = 1.0f;
  int scale = 3;
  uint32_t seed = 42;
  std::vector<cpu_lbm::Obstacle> obstacles;
  cpu_lbm::CollMode collMode = cpu_lbm::CollMode::Slip;
  bool noDefaultObstacle = false;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](const char* name, int n){ if (i+n>=argc){std::fprintf(stderr,"missing value for %s\n",name); print_usage(argv[0]); std::exit(1);} };
    if (a=="--nx" && (need("--nx",1),true)) nx = std::atoi(argv[++i]);
    else if (a=="--ny" && (need("--ny",1),true)) ny = std::atoi(argv[++i]);
    else if ((a=="--particles"||a=="--batch"||a=="-p") && (need(a.c_str(),1),true)) batchCount = std::atoi(argv[++i]);
    else if ((a=="--interval"||a=="--freq"||a=="-i") && (need(a.c_str(),1),true)) intervalSec = float(std::atof(argv[++i]));
    else if (a=="--pulse") pulseMode = true;
    else if (a=="--stream"||a=="--continuous") pulseMode = false;
    else if (a=="--u0" && (need("--u0",1),true)) u0 = float(std::atof(argv[++i]));
    else if (a=="--dt" && (need("--dt",1),true)) dt = float(std::atof(argv[++i]));
    else if (a=="--scale" && (need("--scale",1),true)) scale = std::atoi(argv[++i]);
    else if (a=="--seed" && (need("--seed",1),true)) seed = uint32_t(std::atoi(argv[++i]));
    else if (a=="--circle" && (need("--circle",3),true)) { float cx=float(std::atof(argv[++i])), cy=float(std::atof(argv[++i])), r=float(std::atof(argv[++i])); obstacles.push_back(cpu_lbm::Obstacle::makeCircle(cx,cy,r)); }
    else if (a=="--block" && (need("--block",4),true)) { float cx=float(std::atof(argv[++i])), cy=float(std::atof(argv[++i])), w=float(std::atof(argv[++i])), h=float(std::atof(argv[++i])); obstacles.push_back(cpu_lbm::Obstacle::makeBlock(cx,cy,w,h)); }
    else if (a=="--no-obstacle"||a=="--no-obstacles") noDefaultObstacle=true;
    else if (a=="--coll" && (need("--coll",1),true)) collMode = cpu_lbm::collModeFromString(argv[++i]);
    else if (a=="--help"||a=="-h") { print_usage(argv[0]); return 0; }
    else { std::fprintf(stderr,"unknown arg: %s\n",a.c_str()); print_usage(argv[0]); return 1; }
  }
  if (nx<=0||ny<=0||batchCount<=0||scale<=0){ std::fprintf(stderr,"invalid parameters\n"); return 1; }
  if (obstacles.empty() && !noDefaultObstacle) {
    float cx = float(nx)*0.45f;
    float cy = float(ny)*0.5f;
    float r = std::min(float(nx), float(ny))*0.18f;
    obstacles.push_back(cpu_lbm::Obstacle::makeCircle(cx, cy, r));
  }

  cpu_lbm::Field2D field(nx, ny);
  field.fillUniform(u0, 0.0f);
  cpu_lbm::TracerSet tracers(0, nx, ny, seed);
  tracers.setEmitConfig(batchCount, intervalSec, pulseMode);
  tracers.emitBurst(batchCount);

  try {
    cpu_lbm::FieldViewer viewer(nx, ny, scale, "WTFS — cpu-lbm 0b (uniform + obstacles + pulse stream)");
    cpu_lbm::LiveParams params{batchCount, intervalSec, pulseMode, dt, u0, collMode};
    // Uniform has no physics step
    return cpu_lbm::runLiveLoop(viewer, field, tracers, obstacles, params, nullptr, nullptr);
  } catch (const std::exception &e) {
    std::fprintf(stderr, "viewer error: %s\n", e.what());
    return 1;
  }
}
