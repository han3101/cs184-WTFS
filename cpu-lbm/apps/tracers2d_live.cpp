#define GL_SILENCE_DEPRECATION
#include "field.h"
#include "tracers.h"
#include "units.h"
#include "obstacle.h"
#include "field_viewer.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>
#include <vector>

static void print_usage(const char *prog) {
  std::fprintf(stderr,
    "Usage: %s [options]\n"
    "Phase 0b live viewer — uniform field + obstacles + tracers, now visible.\n"
    "Options (all optional — you can just run and pick in-viewer):\n"
    "  --nx N          grid width  (default %d)\n"
    "  --ny N          grid height (default %d)\n"
    "  --particles N   initial tracer count (default 10000)\n"
    "  --u0 F          uniform velocity ux (default %.3f)\n"
    "  --dt F          advection scale per frame (default 1.0)\n"
    "  --scale N       window scale (default 3)  window = nx*scale x ny*scale\n"
    "  --seed N        RNG seed (default 42)\n"
    "  --circle cx cy r   add circle obstacle at start (may repeat)\n"
    "  --block cx cy w h  add AABB block at start (center + full size)\n"
    "  --no-obstacle      start with no obstacle (default is big circle)\n"
    "  --coll MODE     collision: slip|stick|bounce|passive (default slip)\n"
    "Controls (while running) — pick shape live, flags are just for scripting:\n"
    "  SPACE  pause/resume   R  reseed   +/-  add/remove 100\n"
    "  1  circle   2  block   0  none   B  cycle circle→block→none\n"
    "  C  cycle coll mode (slip→stick→bounce→passive)\n"
    "  ESC/q  quit\n",
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
  std::vector<cpu_lbm::Obstacle> obstacles;
  cpu_lbm::CollMode collMode = cpu_lbm::CollMode::Slip;
  bool noDefaultObstacle = false;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](const char* name, int n){ if (i+n>=argc){std::fprintf(stderr,"missing value for %s\n",name); print_usage(argv[0]); std::exit(1);} };
    if (a=="--nx" && (need("--nx",1),true)) nx = std::atoi(argv[++i]);
    else if (a=="--ny" && (need("--ny",1),true)) ny = std::atoi(argv[++i]);
    else if (a=="--particles" && (need("--particles",1),true)) nParticles = std::atoi(argv[++i]);
    else if (a=="--u0" && (need("--u0",1),true)) u0 = float(std::atof(argv[++i]));
    else if (a=="--dt" && (need("--dt",1),true)) dt = float(std::atof(argv[++i]));
    else if (a=="--scale" && (need("--scale",1),true)) scale = std::atoi(argv[++i]);
    else if (a=="--seed" && (need("--seed",1),true)) seed = uint32_t(std::atoi(argv[++i]));
    else if (a=="--circle" && (need("--circle",3),true)) {
      float cx=float(std::atof(argv[++i])), cy=float(std::atof(argv[++i])), r=float(std::atof(argv[++i]));
      obstacles.push_back(cpu_lbm::Obstacle::makeCircle(cx,cy,r));
    }
    else if (a=="--block" && (need("--block",4),true)) {
      float cx=float(std::atof(argv[++i])), cy=float(std::atof(argv[++i])), w=float(std::atof(argv[++i])), h=float(std::atof(argv[++i]));
      obstacles.push_back(cpu_lbm::Obstacle::makeBlock(cx,cy,w,h));
    }
    else if (a=="--no-obstacle"||a=="--no-obstacles") noDefaultObstacle=true;
    else if (a=="--coll" && (need("--coll",1),true)) collMode = cpu_lbm::collModeFromString(argv[++i]);
    else if (a=="--help"||a=="-h") { print_usage(argv[0]); return 0; }
    else { std::fprintf(stderr,"unknown arg: %s\n",a.c_str()); print_usage(argv[0]); return 1; }
  }
  if (nx<=0||ny<=0||nParticles<0||scale<=0){ std::fprintf(stderr,"invalid nx/ny/particles/scale\n"); return 1; }

  // Default big obstacle if none specified: big circle at tunnel center (visual gate)
  if (obstacles.empty() && !noDefaultObstacle) {
    float cx = float(nx)*0.45f;
    float cy = float(ny)*0.5f;
    float r = std::min(float(nx), float(ny))*0.18f; // big, as requested
    obstacles.push_back(cpu_lbm::Obstacle::makeCircle(cx, cy, r));
  }

  cpu_lbm::Field2D field(nx, ny);
  field.fillUniform(u0, 0.0f);
  cpu_lbm::TracerSet tracers(nParticles, nx, ny, seed);

  try {
    cpu_lbm::FieldViewer viewer(nx, ny, scale, "WTFS — cpu-lbm 0b (uniform + obstacles)");

    auto *win = viewer.window();
    bool paused = false;
    // edge detection for toggle keys
    bool spacePrev=false, rPrev=false, plusPrev=false, minusPrev=false, qPrev=false;
    bool cPrev=false, bPrev=false, k1Prev=false, k2Prev=false, k0Prev=false;

    double lastTime = glfwGetTime();
    double lastFpsTime = lastTime;
    int frames = 0;
    int fps = 0;

    // Track live recycle for title
    double lastRecycleCheck = lastTime;
    uint64_t lastRecycledTotal = tracers.stats().recycled;
    int obstacleShapeIdx = 0; // 0=circle,1=block,2=none cycle

    while (!viewer.shouldClose()) {
      viewer.pollEvents();

      // Input — poll each frame, edge-trigger
      bool spaceNow = glfwGetKey(win, GLFW_KEY_SPACE)==GLFW_PRESS;
      bool rNow = glfwGetKey(win, GLFW_KEY_R)==GLFW_PRESS;
      bool plusNow = glfwGetKey(win, GLFW_KEY_EQUAL)==GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_ADD)==GLFW_PRESS;
      bool minusNow = glfwGetKey(win, GLFW_KEY_MINUS)==GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_SUBTRACT)==GLFW_PRESS;
      bool qNow = glfwGetKey(win, GLFW_KEY_Q)==GLFW_PRESS;
      bool cNow = glfwGetKey(win, GLFW_KEY_C)==GLFW_PRESS;
      bool bNow = glfwGetKey(win, GLFW_KEY_B)==GLFW_PRESS;
      bool k1Now = glfwGetKey(win, GLFW_KEY_1)==GLFW_PRESS;
      bool k2Now = glfwGetKey(win, GLFW_KEY_2)==GLFW_PRESS;
      bool k0Now = glfwGetKey(win, GLFW_KEY_0)==GLFW_PRESS;

      if (spaceNow && !spacePrev) paused = !paused;
      if (qNow && !qPrev) glfwSetWindowShouldClose(win, GLFW_TRUE);
      if (rNow && !rPrev) tracers.reseed(uint32_t(std::rand()));
      if (plusNow && !plusPrev) tracers.add(100);
      if (minusNow && !minusPrev) tracers.remove(100);
      if (cNow && !cPrev) {
        // cycle slip->stick->bounce->passive->slip
        if (collMode==cpu_lbm::CollMode::Slip) collMode=cpu_lbm::CollMode::Stick;
        else if (collMode==cpu_lbm::CollMode::Stick) collMode=cpu_lbm::CollMode::Bounce;
        else if (collMode==cpu_lbm::CollMode::Bounce) collMode=cpu_lbm::CollMode::Passive;
        else collMode=cpu_lbm::CollMode::Slip;
        std::printf("[coll] mode -> %s\n", cpu_lbm::collModeName(collMode));
      }
      if (bNow && !bPrev) {
        obstacleShapeIdx = (obstacleShapeIdx+1)%3;
        obstacles.clear();
        if (obstacleShapeIdx==0) {
          float cx=float(nx)*0.45f, cy=float(ny)*0.5f, r=std::min(float(nx),float(ny))*0.18f;
          obstacles.push_back(cpu_lbm::Obstacle::makeCircle(cx,cy,r));
          std::printf("[obstacle] -> big circle\n");
        } else if (obstacleShapeIdx==1) {
          float cx=float(nx)*0.45f, cy=float(ny)*0.5f, w=float(ny)*0.36f, h=float(ny)*0.36f;
          obstacles.push_back(cpu_lbm::Obstacle::makeBlock(cx,cy,w,h));
          std::printf("[obstacle] -> big block\n");
        } else {
          std::printf("[obstacle] -> none\n");
        }
      }
      if (k1Now && !k1Prev) {
        obstacles.clear();
        float cx=float(nx)*0.45f, cy=float(ny)*0.5f, r=std::min(float(nx),float(ny))*0.18f;
        obstacles.push_back(cpu_lbm::Obstacle::makeCircle(cx,cy,r));
        obstacleShapeIdx=0;
        std::printf("[obstacle] -> big circle (key 1)\n");
      }
      if (k2Now && !k2Prev) {
        obstacles.clear();
        float cx=float(nx)*0.45f, cy=float(ny)*0.5f, w=float(ny)*0.36f, h=float(ny)*0.36f;
        obstacles.push_back(cpu_lbm::Obstacle::makeBlock(cx,cy,w,h));
        obstacleShapeIdx=1;
        std::printf("[obstacle] -> big block (key 2)\n");
      }
      if (k0Now && !k0Prev) {
        obstacles.clear();
        obstacleShapeIdx=2;
        std::printf("[obstacle] -> none (key 0)\n");
      }

      spacePrev=spaceNow; rPrev=rNow; plusPrev=plusNow; minusPrev=minusNow; qPrev=qNow;
      cPrev=cNow; bPrev=bNow; k1Prev=k1Now; k2Prev=k2Now; k0Prev=k0Now;

      if (!paused) tracers.advect(field, dt, obstacles, collMode);

      viewer.beginFrame();
      viewer.drawField(field);
      viewer.drawObstacles(obstacles);
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
        std::snprintf(title, sizeof(title), "WTFS 0b %dx%d  %d tracers  %s  %s  obs=%zu  %d FPS  %.1f ms  recycled %.0f/s  [SPACE pause R reseed +/- C coll B shape]",
          nx, ny, int(tracers.size()), paused?"PAUSED":"running", cpu_lbm::collModeName(collMode), obstacles.size(), fps, dtAvg, recycledPerSec);
        viewer.setTitle(title);
        lastFpsTime = now;
        lastRecycleCheck = now;
        lastRecycledTotal = s.recycled;
        frames = 0;
      }

      (void)lastTime;
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "viewer error: %s\n", e.what());
    std::fprintf(stderr, "Hint: on macOS, install GLFW via 'brew install glfw' or let CMake FetchContent build it (needs network).\n");
    return 1;
  }
  return 0;
}
