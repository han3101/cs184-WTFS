#pragma once
#include "field.h"
#include "tracers.h"
#include "obstacle.h"
#include <string>
#include <vector>
#include <functional>

struct GLFWwindow;

namespace cpu_lbm {

// Phase 0b viewer — GLFW + legacy OpenGL (compatibility profile).
// Linked ONLY by *_live apps so cpu_lbm_core stays headless/testable.
// Features: smooth trackpad pan/zoom + generous bounds + prominent control bar.
// Keeps taste filter: matte neutrals, sharp 1px borders, no gradients/glow.
class FieldViewer {
public:
  // nx,ny = field size in cells. Window size = nx*scale x ny*scale.
  FieldViewer(int nx, int ny, int scale = 3,
              const std::string &title = "WTFS — tracers 0b");
  ~FieldViewer();

  FieldViewer(const FieldViewer &) = delete;
  FieldViewer &operator=(const FieldViewer &) = delete;

  bool shouldClose() const;
  void pollEvents();

  void beginFrame();
  void drawField(const Field2D &field); // 0b: solid clear; field param kept for 0c drop-in
  void drawObstacles(const std::vector<Obstacle> &obs);
  void drawTracers(const TracerSet &tracers);
  void endFrame();

  void setTitle(const std::string &t);
  GLFWwindow *window() const { return window_; }

  int windowWidth() const { return nx_ * scale_; }
  int windowHeight() const { return ny_ * scale_; }

  // Zoom / pan (field space)
  float zoom() const { return zoom_; }
  void setZoom(float z);
  void resetView();
  void zoomAt(double mouseX, double mouseY, float factor); // mouse in window/screen coords

  float controlBarHeight() const { return 36.0f; } // height in framebuffer pixels

  // UI control bar — flat matte buttons, no gradients.
  // App wires callbacks so viewer stays decoupled from TracerSet ownership.
  std::function<void(int)> onParticleDelta;   // manual add/remove
  std::function<void(int)> onBatchDelta;      // e.g. -50 / +50 or -100 / +100 particles per iteration
  std::function<void(float)> onIntervalDelta; // e.g. -0.05s / +0.05s between bursts
  std::function<void(float)> onSpeedDelta;    // multiply dt by factor (e.g. 0.85 / 1.18)
  std::function<void()> onToggleEmission;    // toggle pulse waves vs continuous stream
  std::function<void()> onBurst;             // trigger immediate wave burst
  std::function<void()> onResetView;
  std::function<void()> onTogglePause;
  std::function<void()> onReseed;

  // Draw screen-space control bar; must be called between drawTracers and endFrame.
  // ww,wh = framebuffer size, mouseX/Y in window coords, mousePressed = left down.
  // Returns true if mouse is over UI (so app can suppress panning).
  bool drawControlBar(int framebufferW, int framebufferH,
                      double mouseX, double mouseY, bool mousePressed,
                      int particleCount, float speed, bool paused,
                      int batchCount = 300, float intervalSec = 0.30f,
                      bool pulseMode = true, const char *collModeStr = nullptr);

  // Simple hit-test helper for app-level extensions
  static bool hitTest(float mx, float my, float x, float y, float w, float h);

private:
  int nx_, ny_, scale_;
  GLFWwindow *window_ = nullptr;
  float zoom_ = 1.0f;
  float panX_ = 0.0f, panY_ = 0.0f;
  bool isPanning_ = false;
  double lastMx_ = 0, lastMy_ = 0;
  double lastMouseX_ = 0, lastMouseY_ = 0;
  bool leftDown_ = false;
  bool middleDown_ = false;
  bool rightDown_ = false;
  bool uiMouseDownPrev_ = false;

  static void keyCallback(GLFWwindow *w, int key, int scancode, int action, int mods);
  static void framebufferSizeCallback(GLFWwindow *w, int ww, int wh);
  static void scrollCallback(GLFWwindow *w, double xoff, double yoff);
  static void mouseButtonCallback(GLFWwindow *w, int button, int action, int mods);
  static void cursorPosCallback(GLFWwindow *w, double xpos, double ypos);

  void handleScroll(double xoff, double yoff);
  void beginFieldTransform(int ww, int wh);
  void endFieldTransform();
  void beginUI(int ww, int wh);
  void endUI();
  void drawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f);
  void drawRectBorder(float x, float y, float w, float h, float r, float g, float b);
  void drawText(float x, float y, const char *text, float pixelSize = 1.35f);
  bool drawButton(float x, float y, float w, float h, const char *label,
                  bool hovered, bool pressed);

  // Convert framebuffer pixel (origin top-left from GLFW cursor) to field coords
  void screenToField(double fbMx, double fbMy, int fbW, int fbH, float &fx, float &fy) const;
};

} // namespace cpu_lbm
