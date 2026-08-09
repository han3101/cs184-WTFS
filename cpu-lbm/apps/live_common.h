#pragma once
#define GL_SILENCE_DEPRECATION
#include "field.h"
#include "tracers.h"
#include "obstacle.h"
#include "units.h"
#include "field_viewer.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <cmath>

namespace cpu_lbm {

// Shared live viewer framework — used by both tracers2d_live and cyl2d_live
// so they stay visually and behaviorally identical. Physics is injected via callbacks.

struct LiveParams {
  int batchCount = 300;
  float intervalSec = 0.30f;
  bool pulseMode = true;
  float dt = 1.0f;
  float u0 = U_LB_DEFAULT;
  CollMode collMode = CollMode::Slip;
};

// Wire standard control-bar callbacks (batch, interval, emission, burst, particle delta, speed, pause, reseed)
inline void wireViewerCallbacks(FieldViewer& viewer, TracerSet& tracers, LiveParams& p, bool& paused) {
  viewer.onBatchDelta = [&](int d){
    p.batchCount = std::clamp(p.batchCount + d, 10, 5000);
    tracers.setEmitCount(p.batchCount);
    std::printf("[pulse] batch count -> %d particles\n", p.batchCount);
  };
  viewer.onIntervalDelta = [&](float d){
    p.intervalSec = std::clamp(p.intervalSec + d, 0.02f, 5.0f);
    tracers.setEmitInterval(p.intervalSec);
    std::printf("[pulse] interval -> %.2fs (%.1f Hz)\n", p.intervalSec, 1.0f / p.intervalSec);
  };
  viewer.onToggleEmission = [&](){
    p.pulseMode = !tracers.periodicEmission();
    tracers.setPeriodicEmission(p.pulseMode);
    std::printf("[emission] mode -> %s\n", p.pulseMode ? "PULSE" : "STREAM");
  };
  viewer.onBurst = [&](){
    tracers.emitBurst(p.batchCount);
    std::printf("[burst] emitted %d particles (total: %zu)\n", p.batchCount, tracers.size());
  };
  viewer.onParticleDelta = [&](int d){
    if (d > 0) tracers.add(d); else tracers.remove(-d);
    std::printf("[particles] %+d -> %zu\n", d, tracers.size());
  };
  viewer.onSpeedDelta = [&](float f){
    p.dt = std::clamp(p.dt * f, 0.05f, 5.0f);
    std::printf("[speed] dt -> %.3f (%.3f lattice)\n", p.dt, p.dt * p.u0);
  };
  viewer.onTogglePause = [&](){ paused = !paused; std::printf("[pause] %s\n", paused?"paused":"running"); };
  viewer.onResetView = [&](){ std::printf("[view] reset\n"); };
  viewer.onReseed = [&](){
    tracers.clear();
    tracers.reseed(uint32_t(std::rand()));
    tracers.emitBurst(p.batchCount);
    std::printf("[tracers] reseeded & burst %d\n", p.batchCount);
  };
}

// Shared main loop — physicsStep is called each frame before tracer advect when not paused.
// onObstacleChange is called after B/1/2/0 rebuilds obstacles so caller can rebuild solidMask/tau.
inline int runLiveLoop(FieldViewer& viewer, Field2D& field, TracerSet& tracers,
                       std::vector<Obstacle>& obstacles, LiveParams& params,
                       std::function<void()> physicsStep,
                       std::function<void()> onObstacleChange = nullptr,
                       std::function<std::string(int fps, float mlups)> titleFn = nullptr) {
  auto* win = viewer.window();
  bool paused = false;
  bool spacePrev=false, rPrev=false, plusPrev=false, minusPrev=false, qPrev=false;
  bool cPrev=false, bPrev=false, k1Prev=false, k2Prev=false, k0Prev=false;
  bool lbracketPrev=false, rbracketPrev=false, equalPrev=false, minusKeyPrev=false;
  bool pKeyPrev=false, iKeyPrev=false, tKeyPrev=false, eKeyPrev=false;

  wireViewerCallbacks(viewer, tracers, params, paused);

  double lastTime = glfwGetTime();
  double lastFpsTime = lastTime;
  int frames = 0;
  int fps = 0;
  int obstacleShapeIdx = 0;
  // Derive default shape idx from initial obstacles for B cycling
  if (!obstacles.empty() && obstacles[0].type == Obstacle::AABB) obstacleShapeIdx = 1;

  // Determine field size for obstacle rebuilds
  int nx = field.nx, ny = field.ny;

  while (!viewer.shouldClose()) {
    viewer.pollEvents();
    double currentTime = glfwGetTime();
    double realDt = currentTime - lastTime;
    lastTime = currentTime;
    if (realDt > 0.1) realDt = 0.1;

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
    if (rNow && !rPrev) { tracers.clear(); tracers.reseed(uint32_t(std::rand())); tracers.emitBurst(params.batchCount); }
    if (eKeyNow && !eKeyPrev) tracers.emitBurst(params.batchCount);
    if (tKeyNow && !tKeyPrev) { params.pulseMode = !tracers.periodicEmission(); tracers.setPeriodicEmission(params.pulseMode); std::printf("[emission] mode -> %s\n", params.pulseMode ? "PULSE" : "STREAM"); }
    if (pKeyNow && !pKeyPrev) { int d = shiftDown ? -50 : 50; params.batchCount = std::clamp(params.batchCount + d, 10, 5000); tracers.setEmitCount(params.batchCount); std::printf("[pulse] batch count -> %d\n", params.batchCount); }
    if (iKeyNow && !iKeyPrev) { float d = shiftDown ? -0.05f : 0.05f; params.intervalSec = std::clamp(params.intervalSec + d, 0.02f, 5.0f); tracers.setEmitInterval(params.intervalSec); std::printf("[pulse] interval -> %.2fs\n", params.intervalSec); }
    if (kpPlusNow && !plusPrev) tracers.add(100);
    if (kpMinusNow && !minusPrev) tracers.remove(100);
    if (lbracketNow && !lbracketPrev) { params.dt = std::clamp(params.dt*0.85f, 0.05f, 5.0f); std::printf("[speed] dt -> %.3f\n", params.dt); }
    if (rbracketNow && !rbracketPrev) { params.dt = std::clamp(params.dt*1.18f, 0.05f, 5.0f); std::printf("[speed] dt -> %.3f\n", params.dt); }
    if (equalNow && !equalPrev) { double mx,my; glfwGetCursorPos(win,&mx,&my); viewer.zoomAt(mx,my,1.18f); }
    if (minusKeyNow && !minusKeyPrev) { double mx,my; glfwGetCursorPos(win,&mx,&my); viewer.zoomAt(mx,my,0.85f); }
    if (k0Now && !k0Prev) {
      if (onObstacleChange) { obstacles.clear(); obstacleShapeIdx=2; onObstacleChange(); std::printf("[obstacle] -> none\n"); }
      else viewer.resetView();
      if (!onObstacleChange) viewer.resetView();
    }
    if (cNow && !cPrev) {
      if (params.collMode==CollMode::Slip) params.collMode=CollMode::Stick;
      else if (params.collMode==CollMode::Stick) params.collMode=CollMode::Bounce;
      else if (params.collMode==CollMode::Bounce) params.collMode=CollMode::Passive;
      else params.collMode=CollMode::Slip;
      std::printf("[coll] mode -> %s\n", collModeName(params.collMode));
    }
    if (bNow && !bPrev) {
      obstacleShapeIdx = (obstacleShapeIdx+1)%3;
      obstacles.clear();
      if (obstacleShapeIdx==0) {
        float cx=float(nx)*0.45f, cy=float(ny)*0.5f, r=std::min(float(nx),float(ny))*0.18f;
        // For cyl default geometry, caller may override via onObstacleChange; use generic 0.18*min
        // If field is wide (400x80), 0.18*min gives ~14, close to 0.25*ny=20 — caller will rebuild with correct D if needed
        obstacles.push_back(Obstacle::makeCircle(cx,cy,r));
        std::printf("[obstacle] -> circle\n");
      } else if (obstacleShapeIdx==1) {
        float cx=float(nx)*0.45f, cy=float(ny)*0.5f, w=float(ny)*0.36f, h=float(ny)*0.36f;
        obstacles.push_back(Obstacle::makeBlock(cx,cy,w,h));
        std::printf("[obstacle] -> block\n");
      } else {
        std::printf("[obstacle] -> none\n");
      }
      if (onObstacleChange) onObstacleChange();
    }
    if (k1Now && !k1Prev) {
      obstacles.clear();
      float cx=float(nx)*0.45f, cy=float(ny)*0.5f, r=std::min(float(nx),float(ny))*0.18f;
      obstacles.push_back(Obstacle::makeCircle(cx,cy,r));
      obstacleShapeIdx=0;
      if (onObstacleChange) onObstacleChange();
      std::printf("[obstacle] -> circle (key 1)\n");
    }
    if (k2Now && !k2Prev) {
      obstacles.clear();
      float cx=float(nx)*0.45f, cy=float(ny)*0.5f, w=float(ny)*0.36f, h=float(ny)*0.36f;
      obstacles.push_back(Obstacle::makeBlock(cx,cy,w,h));
      obstacleShapeIdx=1;
      if (onObstacleChange) onObstacleChange();
      std::printf("[obstacle] -> block (key 2)\n");
    }

    spacePrev=spaceNow; rPrev=rNow; plusPrev=kpPlusNow; minusPrev=kpMinusNow; qPrev=qNow;
    cPrev=cNow; bPrev=bNow; k1Prev=k1Now; k2Prev=k2Now; k0Prev=k0Now;
    lbracketPrev=lbracketNow; rbracketPrev=rbracketNow;
    equalPrev=equalNow; minusKeyPrev=minusKeyNow;
    pKeyPrev=pKeyNow; iKeyPrev=iKeyNow; tKeyPrev=tKeyNow; eKeyPrev=eKeyNow;

    if (!paused) {
      if (physicsStep) physicsStep();
      tracers.stepEmission(float(realDt));
      tracers.advect(field, params.dt, obstacles, params.collMode);
    }

    int ww, wh; glfwGetFramebufferSize(win, &ww, &wh);
    double mx,my; glfwGetCursorPos(win,&mx,&my);
    bool leftDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;

    viewer.beginFrame();
    viewer.drawField(field);
    viewer.drawObstacles(obstacles);
    viewer.drawTracers(tracers);
    viewer.drawControlBar(ww, wh, mx, my, leftDown, int(tracers.size()), params.dt*params.u0, paused,
                          params.batchCount, params.intervalSec, tracers.periodicEmission(), collModeName(params.collMode));
    viewer.endFrame();

    frames++;
    double now = glfwGetTime();
    if (now - lastFpsTime >= 1.0) {
      fps = int(frames / (now - lastFpsTime));
      double mlups = (double(field.nx) * double(field.ny) * double(frames)) / (now - lastFpsTime) / 1.0e6;
      if (titleFn) {
        viewer.setTitle(titleFn(fps, float(mlups)));
      } else {
        char title[320];
        std::snprintf(title, sizeof(title), "WTFS %dx%d  %d tracers  [%d/%.2fs %s]  %s  %s  obs=%zu  %.2fx  SPD %.2f  %d FPS",
          field.nx, field.ny, int(tracers.size()), params.batchCount, params.intervalSec, tracers.periodicEmission()?"PULSE":"STREAM",
          paused?"PAUSED":"running", collModeName(params.collMode), obstacles.size(), viewer.zoom(), params.dt*params.u0, fps);
        viewer.setTitle(title);
      }
      lastFpsTime = now;
      frames = 0;
    }
  }
  return 0;
}

} // namespace cpu_lbm
