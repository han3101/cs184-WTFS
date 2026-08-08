#define GL_SILENCE_DEPRECATION
#include "field.h"
#include "tracers.h"
#include "units.h"
#include "field_viewer.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <string>
#include <cmath>

static void print_usage(const char *prog) {
  std::fprintf(stderr,
    "Usage: %s [options]\n"
    "Phase 0b live viewer — same uniform field as tracers2d_headless, now visible.\n"
    "Options:\n"
    "  --nx N          grid width  (default %d)\n"
    "  --ny N          grid height (default %d)\n"
    "  --particles N   initial tracer count (default 10000)\n"
    "  --u0 F          uniform velocity ux (default %.3f)\n"
    "  --dt F          advection scale per frame (default 1.0)\n"
    "  --scale N       window scale (default 3)  window = nx*scale x ny*scale\n"
    "  --seed N        RNG seed (default 42)\n"
    "Controls (while running):\n"
    "  SPACE  pause/resume   R  reseed   +/-  add/remove 100  ESC/q  quit\n",
    prog, cpu_lbm::NX_DEFAULT, cpu_lbm::NY_DEFAULT, cpu_lbm::U_LB_DEFAULT);
}

int main(int argc, char **argv) {
  int nx = cpu_lbm::NX_DEFAULT;
  int ny = cpu_lbm::NY_DEFAULT;
  int nParticles = 10000;
  float u0 = cpu_lbm::U_LB_DEFAULT;
  float dt = 1.0f;
  int scale = 3;
  uint32_t seed = 42;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](const char* name){ if (i+1>=argc){std::fprintf(stderr,"missing value for %s\n",name); print_usage(argv[0]); std::exit(1);} };
    if (a=="--nx" && (need("--nx"),true)) nx = std::atoi(argv[++i]);
    else if (a=="--ny" && (need("--ny"),true)) ny = std::atoi(argv[++i]);
    else if (a=="--particles" && (need("--particles"),true)) nParticles = std::atoi(argv[++i]);
    else if (a=="--u0" && (need("--u0"),true)) u0 = float(std::atof(argv[++i]));
    else if (a=="--dt" && (need("--dt"),true)) dt = float(std::atof(argv[++i]));
    else if (a=="--scale" && (need("--scale"),true)) scale = std::atoi(argv[++i]);
    else if (a=="--seed" && (need("--seed"),true)) seed = uint32_t(std::atoi(argv[++i]));
    else if (a=="--help"||a=="-h") { print_usage(argv[0]); return 0; }
    else { std::fprintf(stderr,"unknown arg: %s\n",a.c_str()); print_usage(argv[0]); return 1; }
  }
  if (nx<=0||ny<=0||nParticles<0||scale<=0){ std::fprintf(stderr,"invalid nx/ny/particles/scale\n"); return 1; }

  cpu_lbm::Field2D field(nx, ny);
  field.fillUniform(u0, 0.0f);
  cpu_lbm::TracerSet tracers(nParticles, nx, ny, seed);

  try {
    cpu_lbm::FieldViewer viewer(nx, ny, scale, "WTFS — cpu-lbm 0b (uniform)");

    auto *win = viewer.window();
    bool paused = false;
    // edge detection for toggle keys
    bool spacePrev=false, rPrev=false, plusPrev=false, minusPrev=false, qPrev=false;

    double lastTime = glfwGetTime();
    double lastFpsTime = lastTime;
    int frames = 0;
    int fps = 0;

    // Track live recycle for title
    double lastRecycleCheck = lastTime;
    uint64_t lastRecycledTotal = tracers.stats().recycled;

    while (!viewer.shouldClose()) {
      viewer.pollEvents();

      // Input — poll each frame, edge-trigger
      bool spaceNow = glfwGetKey(win, GLFW_KEY_SPACE)==GLFW_PRESS;
      bool rNow = glfwGetKey(win, GLFW_KEY_R)==GLFW_PRESS;
      bool plusNow = glfwGetKey(win, GLFW_KEY_EQUAL)==GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_ADD)==GLFW_PRESS;
      bool minusNow = glfwGetKey(win, GLFW_KEY_MINUS)==GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_SUBTRACT)==GLFW_PRESS;
      bool qNow = glfwGetKey(win, GLFW_KEY_Q)==GLFW_PRESS;

      if (spaceNow && !spacePrev) paused = !paused;
      if (qNow && !qPrev) glfwSetWindowShouldClose(win, GLFW_TRUE);
      if (rNow && !rPrev) tracers.reseed(uint32_t(std::rand()));
      if (plusNow && !plusPrev) tracers.add(100);
      if (minusNow && !minusPrev) tracers.remove(100);

      spacePrev=spaceNow; rPrev=rNow; plusPrev=plusNow; minusPrev=minusNow; qPrev=qNow;

      if (!paused) tracers.advect(field, dt);

      viewer.beginFrame();
      viewer.drawField(field);
      viewer.drawTracers(tracers);
      viewer.endFrame();

      // FPS + live stats in title (proves count decoupled, recycle visible)
      frames++;
      double now = glfwGetTime();
      if (now - lastFpsTime >= 1.0) {
        fps = int(frames / (now - lastFpsTime));
        double dtAvg = (frames>0) ? (now - lastFpsTime)/frames*1000.0 : 0;
        auto s = tracers.stats();
        double recycledPerSec = (now > lastRecycleCheck) ? double(s.recycled - lastRecycledTotal)/(now - lastRecycleCheck) : 0;
        char title[256];
        std::snprintf(title, sizeof(title), "WTFS 0b %dx%d  %d tracers  %s  %d FPS  %.1f ms  recycled %.0f/s  [SPACE pause R reseed +/-]",
          nx, ny, int(tracers.size()), paused?"PAUSED":"running", fps, dtAvg, recycledPerSec);
        viewer.setTitle(title);
        lastFpsTime = now;
        lastRecycleCheck = now;
        lastRecycledTotal = s.recycled;
        frames = 0;
      }

      // Also check gate intermittently: stable count is always true by construction
      (void)lastTime;
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "viewer error: %s\n", e.what());
    std::fprintf(stderr, "Hint: on macOS, install GLFW via 'brew install glfw' or let CMake FetchContent build it (needs network).\n");
    return 1;
  }
  return 0;
}
