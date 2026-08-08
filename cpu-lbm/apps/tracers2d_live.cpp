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
#include <algorithm>

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
    "  --coll MODE          collision: slip|stick|bounce|passive (default slip)\n"
    "Controls (while running):\n"
    "  Top bar buttons:\n"
    "    PULSE/STREAM toggle   BATCH -/+ (±50 pts)   FREQ -/+ (±0.05s)   SPD -/+   BURST\n"
    "    RESET view            PLAY/PAUSE\n"
    "  Mouse / Trackpad:\n"
    "    1-finger drag / Left drag: pan field (1:1 cursor lock)\n"
    "    2-finger scroll: smooth zoom at cursor\n"
    "    Shift + 2-finger scroll / swipe: pan field\n"
    "  Keys:\n"
    "    SPACE  pause/resume    R  clear & reseed    E  emit burst\n"
    "    T      toggle pulse/stream mode\n"
    "    P/p    batch size down/up (±50 particles per pulse)\n"
    "    I/i    interval down/up (±0.05s between pulses)\n"
    "    1  circle   2  block   0/RST  reset view   B  cycle obstacles\n"
    "    C  cycle coll mode (slip->stick->bounce->passive)\n"
    "    [ / ]  speed down/up   - / =  zoom out/in\n"
    "    ESC/q  quit\n",
    prog, cpu_lbm::NX_DEFAULT, cpu_lbm::NY_DEFAULT, cpu_lbm::U_LB_DEFAULT);
}

int main(int argc, char **argv) {
  int nx = cpu_lbm::NX_DEFAULT;
  int ny = cpu_lbm::NY_DEFAULT;
  int batchCount = 300;           // default 300 particles per iteration
  float intervalSec = 0.30f;      // default every 0.3 seconds
  bool pulseMode = true;          // default pulsed wave emission
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
  if (nx<=0||ny<=0||batchCount<=0||scale<=0){ std::fprintf(stderr,"invalid parameters\n"); return 1; }

  if (obstacles.empty() && !noDefaultObstacle) {
    float cx = float(nx)*0.45f;
    float cy = float(ny)*0.5f;
    float r = std::min(float(nx), float(ny))*0.18f;
    obstacles.push_back(cpu_lbm::Obstacle::makeCircle(cx, cy, r));
  }

  cpu_lbm::Field2D field(nx, ny);
  field.fillUniform(u0, 0.0f);

  // Initialize TracerSet with pulse configuration (300 particles every 0.3s by default)
  cpu_lbm::TracerSet tracers(0, nx, ny, seed);
  tracers.setEmitConfig(batchCount, intervalSec, pulseMode);
  // Emit initial batch so particles start flowing immediately
  tracers.emitBurst(batchCount);

  try {
    cpu_lbm::FieldViewer viewer(nx, ny, scale, "WTFS — cpu-lbm 0b (uniform + obstacles + pulse stream)");
    auto *win = viewer.window();
    bool paused = false;
    bool spacePrev=false, rPrev=false, plusPrev=false, minusPrev=false, qPrev=false;
    bool cPrev=false, bPrev=false, k1Prev=false, k2Prev=false, k0Prev=false;
    bool lbracketPrev=false, rbracketPrev=false, equalPrev=false, minusKeyPrev=false;
    bool pKeyPrev=false, iKeyPrev=false, tKeyPrev=false, eKeyPrev=false;

    // Wire viewer control bar callbacks
    viewer.onBatchDelta = [&](int d){
      batchCount = std::clamp(batchCount + d, 10, 5000);
      tracers.setEmitCount(batchCount);
      std::printf("[pulse] batch count -> %d particles\n", batchCount);
    };
    viewer.onIntervalDelta = [&](float d){
      intervalSec = std::clamp(intervalSec + d, 0.02f, 5.0f);
      tracers.setEmitInterval(intervalSec);
      std::printf("[pulse] interval -> %.2fs (%.1f Hz)\n", intervalSec, 1.0f / intervalSec);
    };
    viewer.onToggleEmission = [&](){
      pulseMode = !tracers.periodicEmission();
      tracers.setPeriodicEmission(pulseMode);
      std::printf("[emission] mode -> %s\n", pulseMode ? "PULSE" : "STREAM");
    };
    viewer.onBurst = [&](){
      tracers.emitBurst(batchCount);
      std::printf("[burst] emitted %d particles (total: %zu)\n", batchCount, tracers.size());
    };
    viewer.onParticleDelta = [&](int d){
      if (d > 0) tracers.add(d);
      else tracers.remove(-d);
      std::printf("[particles] %+d -> %zu\n", d, tracers.size());
    };
    viewer.onSpeedDelta = [&](float f){
      dt = std::clamp(dt * f, 0.05f, 5.0f);
      std::printf("[speed] dt -> %.3f (%.3f lattice)\n", dt, dt*u0);
    };
    viewer.onTogglePause = [&](){ paused = !paused; std::printf("[pause] %s\n", paused?"paused":"running"); };
    viewer.onResetView = [&](){ std::printf("[view] reset\n"); };
    viewer.onReseed = [&](){
      tracers.clear();
      tracers.reseed(uint32_t(std::rand()));
      tracers.emitBurst(batchCount);
      std::printf("[tracers] reseeded & burst %d\n", batchCount);
    };

    double lastTime = glfwGetTime();
    double lastFpsTime = lastTime;
    int frames = 0;
    int fps = 0;
    double lastRecycleCheck = lastTime;
    uint64_t lastRecycledTotal = tracers.stats().recycled;
    int obstacleShapeIdx = 0;

    while (!viewer.shouldClose()) {
      viewer.pollEvents();

      double currentTime = glfwGetTime();
      double realDt = currentTime - lastTime;
      lastTime = currentTime;
      if (realDt > 0.1) realDt = 0.1; // clamp delta time for timer stability

      bool spaceNow = glfwGetKey(win, GLFW_KEY_SPACE)==GLFW_PRESS;
      bool rNow = glfwGetKey(win, GLFW_KEY_R)==GLFW_PRESS;
      bool kpPlusNow = glfwGetKey(win, GLFW_KEY_KP_ADD)==GLFW_PRESS;
      bool kpMinusNow = glfwGetKey(win, GLFW_KEY_KP_SUBTRACT)==GLFW_PRESS;
      bool qNow = glfwGetKey(win, GLFW_KEY_Q)==GLFW_PRESS;
      bool cNow = glfwGetKey(win, GLFW_KEY_C)==GLFW_PRESS;
      bool bNow = glfwGetKey(win, GLFW_KEY_B)==GLFW_PRESS;
      bool k1Now = glfwGetKey(win, GLFW_KEY_1)==GLFW_PRESS;
      bool k2Now = glfwGetKey(win, GLFW_KEY_2)==GLFW_PRESS;
      bool k0Now = glfwGetKey(win, GLFW_KEY_0)==GLFW_PRESS;
      bool lbracketNow = glfwGetKey(win, GLFW_KEY_LEFT_BRACKET)==GLFW_PRESS;
      bool rbracketNow = glfwGetKey(win, GLFW_KEY_RIGHT_BRACKET)==GLFW_PRESS;
      bool equalNow = glfwGetKey(win, GLFW_KEY_EQUAL)==GLFW_PRESS;
      bool minusKeyNow = glfwGetKey(win, GLFW_KEY_MINUS)==GLFW_PRESS;
      bool pKeyNow = glfwGetKey(win, GLFW_KEY_P)==GLFW_PRESS;
      bool iKeyNow = glfwGetKey(win, GLFW_KEY_I)==GLFW_PRESS;
      bool tKeyNow = glfwGetKey(win, GLFW_KEY_T)==GLFW_PRESS;
      bool eKeyNow = glfwGetKey(win, GLFW_KEY_E)==GLFW_PRESS;
      bool shiftDown = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS || glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT)==GLFW_PRESS);

      if (spaceNow && !spacePrev) paused = !paused;
      if (qNow && !qPrev) glfwSetWindowShouldClose(win, GLFW_TRUE);
      if (rNow && !rPrev) {
        tracers.clear();
        tracers.reseed(uint32_t(std::rand()));
        tracers.emitBurst(batchCount);
      }
      if (eKeyNow && !eKeyPrev) tracers.emitBurst(batchCount);
      if (tKeyNow && !tKeyPrev) {
        pulseMode = !tracers.periodicEmission();
        tracers.setPeriodicEmission(pulseMode);
        std::printf("[emission] mode -> %s\n", pulseMode ? "PULSE" : "STREAM");
      }
      if (pKeyNow && !pKeyPrev) {
        int delta = shiftDown ? -50 : 50;
        batchCount = std::clamp(batchCount + delta, 10, 5000);
        tracers.setEmitCount(batchCount);
        std::printf("[pulse] batch count -> %d\n", batchCount);
      }
      if (iKeyNow && !iKeyPrev) {
        float delta = shiftDown ? -0.05f : 0.05f;
        intervalSec = std::clamp(intervalSec + delta, 0.02f, 5.0f);
        tracers.setEmitInterval(intervalSec);
        std::printf("[pulse] interval -> %.2fs\n", intervalSec);
      }
      if (kpPlusNow && !plusPrev) tracers.add(100);
      if (kpMinusNow && !minusPrev) tracers.remove(100);
      if (lbracketNow && !lbracketPrev) { dt = std::clamp(dt*0.85f, 0.05f, 5.0f); std::printf("[speed] dt -> %.3f\n", dt); }
      if (rbracketNow && !rbracketPrev) { dt = std::clamp(dt*1.18f, 0.05f, 5.0f); std::printf("[speed] dt -> %.3f\n", dt); }

      if (equalNow && !equalPrev) { double mx,my; glfwGetCursorPos(win,&mx,&my); viewer.zoomAt(mx,my,1.18f); }
      if (minusKeyNow && !minusKeyPrev) { double mx,my; glfwGetCursorPos(win,&mx,&my); viewer.zoomAt(mx,my,0.85f); }
      if (k0Now && !k0Prev) viewer.resetView();
      if (cNow && !cPrev) {
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

      spacePrev=spaceNow; rPrev=rNow; plusPrev=kpPlusNow; minusPrev=kpMinusNow; qPrev=qNow;
      cPrev=cNow; bPrev=bNow; k1Prev=k1Now; k2Prev=k2Now; k0Prev=k0Now;
      lbracketPrev=lbracketNow; rbracketPrev=rbracketNow;
      equalPrev=equalNow; minusKeyPrev=minusKeyNow;
      pKeyPrev=pKeyNow; iKeyPrev=iKeyNow; tKeyPrev=tKeyNow; eKeyPrev=eKeyNow;

      // Step periodic emission timer and advect particles
      if (!paused) {
        tracers.stepEmission(float(realDt));
        tracers.advect(field, dt, obstacles, collMode);
      }

      int ww, wh; glfwGetFramebufferSize(win, &ww, &wh);
      double mx,my; glfwGetCursorPos(win,&mx,&my);
      bool leftDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;

      viewer.beginFrame();
      viewer.drawField(field);
      viewer.drawObstacles(obstacles);
      viewer.drawTracers(tracers);
      viewer.drawControlBar(ww, wh, mx, my, leftDown, int(tracers.size()), dt*u0, paused,
                            batchCount, intervalSec, tracers.periodicEmission(), cpu_lbm::collModeName(collMode));
      viewer.endFrame();

      frames++;
      double now = glfwGetTime();
      if (now - lastFpsTime >= 1.0) {
        fps = int(frames / (now - lastFpsTime));
        double dtAvg = (frames>0) ? (now - lastFpsTime)/frames*1000.0 : 0;
        auto s = tracers.stats();
        double recycledPerSec = (now > lastRecycleCheck) ? double(s.recycled - lastRecycledTotal)/(now - lastRecycleCheck) : 0;
        char title[320];
        std::snprintf(title, sizeof(title), "WTFS 0b %dx%d  %d tracers  [%d/%.2fs %s]  %s  %s  obs=%zu  %.2fx  SPD %.2f  %d FPS  %.1f ms",
          nx, ny, int(tracers.size()), batchCount, intervalSec, tracers.periodicEmission()?"PULSE":"STREAM",
          paused?"PAUSED":"running", cpu_lbm::collModeName(collMode), obstacles.size(), viewer.zoom(), dt*u0, fps, dtAvg);
        viewer.setTitle(title);
        lastFpsTime = now;
        lastRecycleCheck = now;
        lastRecycledTotal = s.recycled;
        frames = 0;
      }
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "viewer error: %s\n", e.what());
    return 1;
  }
  return 0;
}
