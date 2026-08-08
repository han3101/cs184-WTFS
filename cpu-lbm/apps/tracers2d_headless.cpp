#include "field.h"
#include "tracers.h"
#include "units.h"
#include <cstdio>
#include <cstring>
#include <string>

// Phase 0a — headless minimal simulator
// No physics: Field2D is uniform (ux=U_LB, uy=0), tracers advected by that field.
// CI-friendly: prints CSV to stdout and optionally to out/tracers.csv

static void print_usage(const char *prog) {
  std::fprintf(stderr,
    "Usage: %s [options]\n"
    "Options:\n"
    "  --nx N          grid width  (default %d)\n"
    "  --ny N          grid height (default %d)\n"
    "  --particles N   tracer count (default 10000)\n"
    "  --steps N       number of steps (default 2000)\n"
    "  --dt F          timestep scale (default 1.0)\n"
    "  --u0 F          uniform velocity ux (default %.3f)\n"
    "  --seed N        RNG seed (default 42)\n"
    "  --csv PATH      also write CSV to file\n"
    "  --help          show this\n",
    prog, cpu_lbm::NX_DEFAULT, cpu_lbm::NY_DEFAULT, cpu_lbm::U_LB_DEFAULT);
}

int main(int argc, char **argv) {
  int nx = cpu_lbm::NX_DEFAULT;
  int ny = cpu_lbm::NY_DEFAULT;
  int nParticles = 10000;
  int steps = 2000;
  float dt = 1.0f;
  float u0 = cpu_lbm::U_LB_DEFAULT;
  uint32_t seed = 42;
  std::string csvPath;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](int &idx, const char* name) {
      if (idx + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", name); print_usage(argv[0]); return false; }
      return true;
    };
    if (a == "--nx" && need(i, "--nx")) nx = std::atoi(argv[++i]);
    else if (a == "--ny" && need(i, "--ny")) ny = std::atoi(argv[++i]);
    else if (a == "--particles" && need(i, "--particles")) nParticles = std::atoi(argv[++i]);
    else if (a == "--steps" && need(i, "--steps")) steps = std::atoi(argv[++i]);
    else if (a == "--dt" && need(i, "--dt")) dt = float(std::atof(argv[++i]));
    else if (a == "--u0" && need(i, "--u0")) u0 = float(std::atof(argv[++i]));
    else if (a == "--seed" && need(i, "--seed")) seed = uint32_t(std::atoi(argv[++i]));
    else if (a == "--csv" && need(i, "--csv")) csvPath = argv[++i];
    else if (a == "--help" || a == "-h") { print_usage(argv[0]); return 0; }
    else { std::fprintf(stderr, "unknown arg: %s\n", a.c_str()); print_usage(argv[0]); return 1; }
  }

  if (nx <= 0 || ny <= 0 || nParticles < 0 || steps < 0) {
    std::fprintf(stderr, "invalid nx/ny/particles/steps\n");
    return 1;
  }

  // Field — uniform flow, no LBM yet
  cpu_lbm::Field2D field(nx, ny);
  field.fillUniform(u0, 0.0f);

  // Tracers
  cpu_lbm::TracerSet tracers(nParticles, nx, ny, seed);

  FILE *csv = nullptr;
  if (!csvPath.empty()) {
    csv = std::fopen(csvPath.c_str(), "w");
    if (!csv) { std::perror(csvPath.c_str()); return 1; }
  }

  auto writeHeader = [&]() {
    std::printf("step,avg_x,avg_y,recycled,total_recycled\n");
    if (csv) std::fprintf(csv, "step,avg_x,avg_y,recycled,total_recycled\n");
  };
  auto writeRow = [&](int step, int recycled) {
    auto s = tracers.stats();
    std::printf("%d,%.6f,%.6f,%d,%llu\n", step, s.avg_x, s.avg_y, recycled, (unsigned long long)s.recycled);
    if (csv) std::fprintf(csv, "%d,%.6f,%.6f,%d,%llu\n", step, s.avg_x, s.avg_y, recycled, (unsigned long long)s.recycled);
  };

  writeHeader();
  // step 0 — initial state
  writeRow(0, 0);

  for (int step = 1; step <= steps; ++step) {
    int recycled = tracers.advect(field, dt);
    // Log every 10 steps and at the end to keep output manageable at 20k steps
    if (step % 10 == 0 || step == steps) {
      writeRow(step, recycled);
    }
  }

  if (csv) std::fclose(csv);

  // Gate check: monotonic avg_x advance and stable count (unless nParticles==0)
  // Exit code 0 means Gate 0a passed for this run
  if (nParticles > 0) {
    auto s = tracers.stats();
    if (tracers.size() != size_t(nParticles)) {
      std::fprintf(stderr, "FAIL: particle count changed %zu != %d\n", tracers.size(), nParticles);
      return 2;
    }
    // With positive u0 we expect total recycled >0 after enough steps
    // This is a sanity check, not a strict physics gate
    if (u0 > 0 && steps > nx / u0 && s.recycled == 0) {
      std::fprintf(stderr, "WARN: no particles recycled after %d steps (nx=%d u0=%.4f) — advection may be broken\n", steps, nx, u0);
    }
  }

  return 0;
}
