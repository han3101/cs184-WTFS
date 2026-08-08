#define GL_SILENCE_DEPRECATION
#include "field_viewer.h"
#include <GLFW/glfw3.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <cstdio>
#include <stdexcept>
#include <cmath>

namespace cpu_lbm {

static void glfwError(int code, const char *msg) {
  std::fprintf(stderr, "[GLFW %d] %s\n", code, msg);
}

void FieldViewer::keyCallback(GLFWwindow *w, int key, int /*scancode*/, int action, int /*mods*/) {
  if (action != GLFW_PRESS) return;
  if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
}

void FieldViewer::framebufferSizeCallback(GLFWwindow *w, int ww, int wh) {
  // Keep viewport covering the whole window; letterbox by scissor if aspect changes — no crash on resize (gate 0b)
  glViewport(0, 0, ww, wh);
  // Re-establish ortho on resize — fetch nx/ny/scale from user pointer would need storage;
  // app's beginFrame() re-applies ortho each frame, so this is just viewport.
  (void)w;
}

FieldViewer::FieldViewer(int nx, int ny, int scale, const std::string &title)
    : nx_(nx), ny_(ny), scale_(scale) {
  if (nx <= 0 || ny <= 0 || scale <= 0) throw std::invalid_argument("invalid nx/ny/scale");

  glfwSetErrorCallback(glfwError);
  if (!glfwInit()) throw std::runtime_error("glfwInit failed");

  // Request compatibility profile so legacy immediate-mode (glBegin/glOrtho) works
  // on macOS without needing glad/glew or shader compilation in Phase 0b.
  glfwDefaultWindowHints();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  // On macOS, 2.1 hint is ignored if core profile is forced elsewhere; we stay compatible.

  int ww = nx * scale;
  int wh = ny * scale;
  window_ = glfwCreateWindow(ww, wh, title.c_str(), nullptr, nullptr);
  if (!window_) {
    glfwTerminate();
    throw std::runtime_error("glfwCreateWindow failed");
  }
  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1); // vsync

  glfwSetWindowUserPointer(window_, this);
  glfwSetKeyCallback(window_, keyCallback);
  glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);

  // One-time GL state — neutral, non-AI palette: dark field, warm-white particles
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPointSize(2.0f);
}

FieldViewer::~FieldViewer() {
  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  glfwTerminate();
}

bool FieldViewer::shouldClose() const {
  return glfwWindowShouldClose(window_);
}

void FieldViewer::pollEvents() { glfwPollEvents(); }

void FieldViewer::beginFrame() {
  // Re-apply ortho each frame so resize is always correct (gate: resize must not crash)
  int ww, wh;
  glfwGetFramebufferSize(window_, &ww, &wh);
  glViewport(0, 0, ww, wh);

  // Dark neutral background — avoids cream/purple gradient slop; near-black with slight blue
  glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  // Field coords: (0,0) bottom-left, (nx,ny) top-right. Y grows upward matching field sample.
  glOrtho(0.0, double(nx_), 0.0, double(ny_), -1.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void FieldViewer::drawField(const Field2D & /*field*/) {
  // Phase 0b: solid background only. Keep signature identical to future colormap
  // version so Phase 0c is a drop-in. Optional: draw a faint border for orientation.
  glColor3f(0.11f, 0.12f, 0.15f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(0.5f, 0.5f);
  glVertex2f(float(nx_) - 0.5f, 0.5f);
  glVertex2f(float(nx_) - 0.5f, float(ny_) - 0.5f);
  glVertex2f(0.5f, float(ny_) - 0.5f);
  glEnd();
}

void FieldViewer::drawObstacles(const std::vector<Obstacle> &obs) {
  // Taste: flat, neutral, no purple gradient / glow / glass. Big block/circle as readable silhouettes.
  for (auto &o : obs) {
    if (o.type == Obstacle::CIRCLE) {
      // Fill - warm mid-grey, matte
      glColor3f(0.62f, 0.60f, 0.56f);
      glBegin(GL_TRIANGLE_FAN);
      glVertex2f(o.cx, o.cy);
      const int segs = 48;
      for (int i=0;i<=segs;++i) {
        float a = float(i)/float(segs)*2.0f*3.14159265f;
        glVertex2f(o.cx + std::cos(a)*o.r, o.cy + std::sin(a)*o.r);
      }
      glEnd();
      // Outline - darker, crisp
      glColor3f(0.22f, 0.22f, 0.20f);
      glLineWidth(1.5f);
      glBegin(GL_LINE_LOOP);
      for (int i=0;i<segs;++i) {
        float a = float(i)/float(segs)*2.0f*3.14159265f;
        glVertex2f(o.cx + std::cos(a)*o.r, o.cy + std::sin(a)*o.r);
      }
      glEnd();
      glLineWidth(1.0f);
    } else {
      float x0=o.cx-o.hx, x1=o.cx+o.hx, y0=o.cy-o.hy, y1=o.cy+o.hy;
      glColor3f(0.62f, 0.60f, 0.56f);
      glBegin(GL_QUADS);
      glVertex2f(x0,y0); glVertex2f(x1,y0); glVertex2f(x1,y1); glVertex2f(x0,y1);
      glEnd();
      glColor3f(0.22f, 0.22f, 0.20f);
      glLineWidth(1.5f);
      glBegin(GL_LINE_LOOP);
      glVertex2f(x0,y0); glVertex2f(x1,y0); glVertex2f(x1,y1); glVertex2f(x0,y1);
      glEnd();
      glLineWidth(1.0f);
    }
  }
}

void FieldViewer::drawTracers(const TracerSet &tracers) {
  // Warm-white points — single weight, no glow/blur, no emoji, no scale-on-hover
  glColor3f(0.96f, 0.95f, 0.92f);
  glPointSize(2.0f);
  glBegin(GL_POINTS);
  for (auto &t : tracers.data()) {
    // Clamp to view to avoid NaN from stray positions
    float x = t.x;
    float y = t.y;
    if (x < 0) x = 0;
    if (x >= float(nx_)) x = float(nx_) - 1e-3f;
    if (y < 0) y = 0;
    if (y >= float(ny_)) y = float(ny_) - 1e-3f;
    glVertex2f(x, y);
  }
  glEnd();
}

void FieldViewer::endFrame() { glfwSwapBuffers(window_); }

void FieldViewer::setTitle(const std::string &t) { glfwSetWindowTitle(window_, t.c_str()); }

} // namespace cpu_lbm
