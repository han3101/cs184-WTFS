#pragma once
#include "field.h"
#include "tracers.h"
#include <string>

struct GLFWwindow;

namespace cpu_lbm {

// Phase 0b viewer — GLFW + legacy OpenGL (compatibility profile).
// Linked ONLY by *_live apps so cpu_lbm_core stays headless/testable.
// Renders a neutral dark field + white point tracers; no shaders in 0b
// (solid background is the gate — colormap lands in Phase D).
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
  void drawTracers(const TracerSet &tracers);
  void endFrame();

  void setTitle(const std::string &t);
  GLFWwindow *window() const { return window_; }

  int windowWidth() const { return nx_ * scale_; }
  int windowHeight() const { return ny_ * scale_; }

private:
  int nx_, ny_, scale_;
  GLFWwindow *window_ = nullptr;
  static void keyCallback(GLFWwindow *w, int key, int scancode, int action, int mods);
  static void framebufferSizeCallback(GLFWwindow *w, int ww, int wh);
};

} // namespace cpu_lbm
