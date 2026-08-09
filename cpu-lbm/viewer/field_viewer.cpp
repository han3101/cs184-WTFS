#define GL_SILENCE_DEPRECATION
#include "field_viewer.h"
#include <GLFW/glfw3.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <array>
#include <string>
#include <vector>
#include <algorithm>

namespace cpu_lbm {

// ----- 5x7 bitmap font (uppercase + digits + symbols) -----
// Each char: 7 rows, 5 columns. '1' = pixel on.
// Stored as 5-char strings, row0 = top.
static const std::unordered_map<char, std::array<std::string,7>> kFont = {
  {'0', {"01110","10001","10011","10101","11001","10001","01110"}},
  {'1', {"00100","01100","00100","00100","00100","00100","01110"}},
  {'2', {"01110","10001","00001","00010","00100","01000","11111"}},
  {'3', {"11111","00010","00100","00010","00001","10001","01110"}},
  {'4', {"00010","00110","01010","10010","11111","00010","00010"}},
  {'5', {"11111","10000","11110","00001","00001","10001","01110"}},
  {'6', {"01110","10001","10000","11110","10001","10001","01110"}},
  {'7', {"11111","00001","00010","00100","01000","01000","01000"}},
  {'8', {"01110","10001","10001","01110","10001","10001","01110"}},
  {'9', {"01110","10001","10001","01111","00001","00001","01110"}},
  {'A', {"01110","10001","10001","11111","10001","10001","10001"}},
  {'B', {"11110","10001","10001","11110","10001","10001","11110"}},
  {'C', {"01110","10001","10000","10000","10000","10001","01110"}},
  {'D', {"11110","10001","10001","10001","10001","10001","11110"}},
  {'E', {"11111","10000","11110","10000","10000","10000","11111"}},
  {'F', {"11111","10000","11110","10000","10000","10000","10000"}},
  {'G', {"01110","10001","10000","10111","10001","10001","01110"}},
  {'H', {"10001","10001","10001","11111","10001","10001","10001"}},
  {'I', {"01110","00100","00100","00100","00100","00100","01110"}},
  {'J', {"00111","00010","00010","00010","10010","10010","01100"}},
  {'K', {"10001","10010","10100","11000","10100","10010","10001"}},
  {'L', {"10000","10000","10000","10000","10000","10000","11111"}},
  {'M', {"10001","11011","10101","10101","10001","10001","10001"}},
  {'N', {"10001","11001","10101","10011","10001","10001","10001"}},
  {'O', {"01110","10001","10001","10001","10001","10001","01110"}},
  {'P', {"11110","10001","10001","11110","10000","10000","10000"}},
  {'Q', {"01110","10001","10001","10001","10101","10010","01101"}},
  {'R', {"11110","10001","10001","11110","10100","10010","10001"}},
  {'S', {"01111","10000","01110","00001","00001","10001","01110"}},
  {'T', {"11111","00100","00100","00100","00100","00100","00100"}},
  {'U', {"10001","10001","10001","10001","10001","10001","01110"}},
  {'V', {"10001","10001","10001","01010","01010","00100","00100"}},
  {'W', {"10001","10001","10001","10101","10101","11011","10001"}},
  {'X', {"10001","01010","00100","01010","10001","10001","10001"}},
  {'Y', {"10001","01010","00100","00100","00100","00100","00100"}},
  {'Z', {"11111","00001","00010","00100","01000","10000","11111"}},
  {'+', {"00000","00100","00100","11111","00100","00100","00000"}},
  {'-', {"00000","00000","00000","11111","00000","00000","00000"}},
  {'.', {"00000","00000","00000","00000","00000","01100","01100"}},
  {':', {"00000","01100","01100","00000","01100","01100","00000"}},
  {'%', {"11001","11010","00010","00100","01000","10110","10011"}},
  {'/', {"00001","00010","00010","00100","01000","01000","10000"}},
  {'(', {"00010","00100","01000","01000","01000","00100","00010"}},
  {')', {"01000","00100","00010","00010","00010","00100","01000"}},
  {'[', {"01110","01000","01000","01000","01000","01000","01110"}},
  {']', {"01110","00010","00010","00010","00010","00010","01110"}},
  {'|', {"00100","00100","00100","00100","00100","00100","00100"}},
  {',', {"00000","00000","00000","00000","00110","00100","01000"}},
  {'<', {"00010","00100","01000","10000","01000","00100","00010"}},
  {'>', {"01000","00100","00010","00001","00010","00100","01000"}},
  {'?', {"01110","10001","00001","00010","00100","00000","00100"}},
  {'!', {"00100","00100","00100","00100","00100","00000","00100"}},
  {' ', {"00000","00000","00000","00000","00000","00000","00000"}},
};

static void glfwError(int code, const char *msg) {
  std::fprintf(stderr, "[GLFW %d] %s\n", code, msg);
}

void FieldViewer::keyCallback(GLFWwindow *w, int key, int /*scancode*/, int action, int /*mods*/) {
  if (action != GLFW_PRESS) return;
  if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
}

void FieldViewer::framebufferSizeCallback(GLFWwindow *w, int ww, int wh) {
  glViewport(0, 0, ww, wh);
  (void)w;
}

void FieldViewer::scrollCallback(GLFWwindow *w, double xoff, double yoff) {
  auto *self = static_cast<FieldViewer*>(glfwGetWindowUserPointer(w));
  if (self) self->handleScroll(xoff, yoff);
}

void FieldViewer::mouseButtonCallback(GLFWwindow *w, int button, int action, int mods) {
  auto *self = static_cast<FieldViewer*>(glfwGetWindowUserPointer(w));
  if (!self) return;
  double x, y;
  glfwGetCursorPos(w, &x, &y);

  if (action == GLFW_PRESS) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
      self->leftDown_ = true;
      int winW, winH, fbW, fbH;
      glfwGetWindowSize(w, &winW, &winH);
      glfwGetFramebufferSize(w, &fbW, &fbH);
      double sy = (winH > 0) ? (double(fbH) / double(winH)) : 1.0;
      double fbY = y * sy;
      // If click is outside the top control bar, start field panning!
      if (fbY > self->controlBarHeight()) {
        self->isPanning_ = true;
        self->lastMx_ = x;
        self->lastMy_ = y;
      }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT || button == GLFW_MOUSE_BUTTON_MIDDLE) {
      if (button == GLFW_MOUSE_BUTTON_RIGHT) self->rightDown_ = true;
      if (button == GLFW_MOUSE_BUTTON_MIDDLE) self->middleDown_ = true;
      self->isPanning_ = true;
      self->lastMx_ = x;
      self->lastMy_ = y;
    }
  } else if (action == GLFW_RELEASE) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) self->leftDown_ = false;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) self->rightDown_ = false;
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) self->middleDown_ = false;
    if (!self->leftDown_ && !self->rightDown_ && !self->middleDown_) {
      self->isPanning_ = false;
    }
  }
  (void)mods;
}

void FieldViewer::cursorPosCallback(GLFWwindow *w, double xpos, double ypos) {
  auto *self = static_cast<FieldViewer*>(glfwGetWindowUserPointer(w));
  if (!self) return;
  if (self->isPanning_) {
    int winW, winH;
    glfwGetWindowSize(w, &winW, &winH);
    if (winW > 0 && winH > 0) {
      double dx = xpos - self->lastMx_;
      double dy = ypos - self->lastMy_;
      // Map cursor movement directly to field space with 1:1 tracking
      float fppX = float(self->nx_) / float(winW) / self->zoom_;
      float fppY = float(self->ny_) / float(winH) / self->zoom_;
      self->panX_ += float(dx) * fppX;
      // In GLFW, y increases downward; in OpenGL field, y increases upward
      self->panY_ -= float(dy) * fppY;
      // Generous clamp bounds so user can pan across large/zoomed domains
      float maxPanX = float(self->nx_) * 2.5f;
      float maxPanY = float(self->ny_) * 2.5f;
      self->panX_ = std::clamp(self->panX_, -maxPanX, maxPanX);
      self->panY_ = std::clamp(self->panY_, -maxPanY, maxPanY);
      self->lastMx_ = xpos;
      self->lastMy_ = ypos;
    }
  }
  self->lastMouseX_ = xpos;
  self->lastMouseY_ = ypos;
}

FieldViewer::FieldViewer(int nx, int ny, int scale, const std::string &title)
    : nx_(nx), ny_(ny), scale_(scale) {
  if (nx <= 0 || ny <= 0 || scale <= 0) throw std::invalid_argument("invalid nx/ny/scale");

  glfwSetErrorCallback(glfwError);
  if (!glfwInit()) throw std::runtime_error("glfwInit failed");

  glfwDefaultWindowHints();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  int ww = nx * scale;
  int wh = ny * scale;
  window_ = glfwCreateWindow(ww, wh, title.c_str(), nullptr, nullptr);
  if (!window_) {
    glfwTerminate();
    throw std::runtime_error("glfwCreateWindow failed");
  }
  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);

  glfwSetWindowUserPointer(window_, this);
  glfwSetKeyCallback(window_, keyCallback);
  glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
  glfwSetScrollCallback(window_, scrollCallback);
  glfwSetMouseButtonCallback(window_, mouseButtonCallback);
  glfwSetCursorPosCallback(window_, cursorPosCallback);

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

bool FieldViewer::shouldClose() const { return glfwWindowShouldClose(window_); }
void FieldViewer::pollEvents() { glfwPollEvents(); }

void FieldViewer::setZoom(float z) {
  zoom_ = std::clamp(z, 0.20f, 16.0f);
}
void FieldViewer::resetView() { zoom_ = 1.0f; panX_ = 0.0f; panY_ = 0.0f; }

void FieldViewer::screenToField(double fbMx, double fbMy, int fbW, int fbH, float &fx, float &fy) const {
  if (fbW <= 0 || fbH <= 0) { fx = 0; fy = 0; return; }
  double orthoX = (fbMx / double(fbW)) * double(nx_);
  double orthoY = (1.0 - fbMy / double(fbH)) * double(ny_);
  double cx = nx_ * 0.5, cy = ny_ * 0.5;
  fx = float((orthoX - cx - panX_) / zoom_ + cx);
  fy = float((orthoY - cy - panY_) / zoom_ + cy);
}

void FieldViewer::zoomAt(double mx, double my, float factor) {
  int winW, winH, fbW, fbH;
  glfwGetWindowSize(window_, &winW, &winH);
  glfwGetFramebufferSize(window_, &fbW, &fbH);
  if (winW <= 0 || winH <= 0 || fbW <= 0 || fbH <= 0) return;

  double scaleX = double(fbW) / double(winW);
  double scaleY = double(fbH) / double(winH);
  double fbMx = mx * scaleX;
  double fbMy = my * scaleY;

  float fx, fy;
  screenToField(fbMx, fbMy, fbW, fbH, fx, fy);

  float newZoom = std::clamp(zoom_ * factor, 0.20f, 16.0f);
  if (std::abs(newZoom - zoom_) < 1e-5f) return;

  // Keep point (fx, fy) fixed under cursor
  double cx = nx_ * 0.5, cy = ny_ * 0.5;
  panX_ = float((fx - cx) * (zoom_ - newZoom) + panX_);
  panY_ = float((fy - cy) * (zoom_ - newZoom) + panY_);
  zoom_ = newZoom;

  float maxPanX = float(nx_) * 2.5f;
  float maxPanY = float(ny_) * 2.5f;
  panX_ = std::clamp(panX_, -maxPanX, maxPanX);
  panY_ = std::clamp(panY_, -maxPanY, maxPanY);
}

void FieldViewer::handleScroll(double xoff, double yoff) {
  if (xoff == 0 && yoff == 0) return;

  bool shiftDown = (glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                    glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

  int winW, winH;
  glfwGetWindowSize(window_, &winW, &winH);
  if (winW <= 0 || winH <= 0) return;

  // Trackpad 2-finger swipe / Shift-scroll pan
  if (shiftDown || (std::abs(xoff) > std::abs(yoff) && std::abs(xoff) > 0.02)) {
    float fppX = float(nx_) / float(winW) / zoom_;
    float fppY = float(ny_) / float(winH) / zoom_;
    float scrollPanSpeed = 10.0f;
    double panDx = (shiftDown ? -yoff : xoff);
    double panDy = (shiftDown ? 0.0 : yoff);
    panX_ += float(panDx) * fppX * scrollPanSpeed;
    panY_ += float(panDy) * fppY * scrollPanSpeed;
    float maxPanX = float(nx_) * 2.5f;
    float maxPanY = float(ny_) * 2.5f;
    panX_ = std::clamp(panX_, -maxPanX, maxPanX);
    panY_ = std::clamp(panY_, -maxPanY, maxPanY);
  } else {
    // Smooth trackpad / wheel zoom centered at cursor
    double mx = lastMouseX_, my = lastMouseY_;
    float factor = std::pow(1.08f, float(yoff));
    zoomAt(mx, my, factor);
  }
}

void FieldViewer::beginFieldTransform(int ww, int wh) {
  (void)ww; (void)wh;
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0, double(nx_), 0.0, double(ny_), -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  float cx = nx_ * 0.5f, cy = ny_ * 0.5f;
  glTranslatef(cx + panX_, cy + panY_, 0.0f);
  glScalef(zoom_, zoom_, 1.0f);
  glTranslatef(-cx, -cy, 0.0f);
}

void FieldViewer::endFieldTransform() {
  glMatrixMode(GL_PROJECTION); glPopMatrix();
  glMatrixMode(GL_MODELVIEW); glPopMatrix();
}

void FieldViewer::beginUI(int ww, int wh) {
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0, double(ww), 0.0, double(wh), -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
}

void FieldViewer::endUI() {
  glMatrixMode(GL_PROJECTION); glPopMatrix();
  glMatrixMode(GL_MODELVIEW); glPopMatrix();
}

void FieldViewer::beginFrame() {
  int ww, wh;
  glfwGetFramebufferSize(window_, &ww, &wh);
  glViewport(0, 0, ww, wh);
  glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  beginFieldTransform(ww, wh);
}

void FieldViewer::drawField(const Field2D & /*field*/) {
  glColor3f(0.11f, 0.12f, 0.15f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(0.5f, 0.5f);
  glVertex2f(float(nx_) - 0.5f, 0.5f);
  glVertex2f(float(nx_) - 0.5f, float(ny_) - 0.5f);
  glVertex2f(0.5f, float(ny_) - 0.5f);
  glEnd();
}

void FieldViewer::drawObstacles(const std::vector<Obstacle> &obs) {
  for (auto &o : obs) {
    if (o.type == Obstacle::CIRCLE) {
      glColor3f(0.62f, 0.60f, 0.56f);
      glBegin(GL_TRIANGLE_FAN);
      glVertex2f(o.cx, o.cy);
      const int segs = 48;
      for (int i=0;i<=segs;++i) {
        float a = float(i)/float(segs)*2.0f*3.14159265f;
        glVertex2f(o.cx + std::cos(a)*o.r, o.cy + std::sin(a)*o.r);
      }
      glEnd();
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
  glColor3f(0.96f, 0.95f, 0.92f);
  glPointSize(2.0f);
  glBegin(GL_POINTS);
  for (auto &t : tracers.data()) {
    float x = t.x, y = t.y;
    if (x < 0) x = 0;
    if (x >= float(nx_)) x = float(nx_) - 1e-3f;
    if (y < 0) y = 0;
    if (y >= float(ny_)) y = float(ny_) - 1e-3f;
    glVertex2f(x, y);
  }
  glEnd();
}

void FieldViewer::drawRect(float x,float y,float w,float h,float r,float g,float b,float a){
  glColor4f(r,g,b,a);
  glBegin(GL_QUADS);
  glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
  glEnd();
}

void FieldViewer::drawRectBorder(float x,float y,float w,float h,float r,float g,float b){
  glColor3f(r,g,b);
  glLineWidth(1.0f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
  glEnd();
}

void FieldViewer::drawText(float x,float y,const char *text,float pixelSize){
  if (!text || !*text) return;
  float cx = x;
  float py = y;
  for (const char *c=text; *c; ++c){
    char ch = *c;
    if (ch>='a' && ch<='z') ch = char(ch - 'a' + 'A');
    if (ch==' ') { cx += 6*pixelSize; continue; }
    auto it = kFont.find(ch);
    if (it==kFont.end()) { cx += 6*pixelSize; continue; }
    const auto &rows = it->second;
    for (int r=0;r<7;++r){
      const std::string &row = rows[r];
      for (int col=0;col<5;++col){
        if (row[col]=='1'){
          float px = cx + col*pixelSize;
          float py2 = py + (6 - r)*pixelSize;
          glBegin(GL_QUADS);
          glVertex2f(px, py2); glVertex2f(px+pixelSize, py2);
          glVertex2f(px+pixelSize, py2+pixelSize); glVertex2f(px, py2+pixelSize);
          glEnd();
        }
      }
    }
    cx += 6*pixelSize;
  }
}

bool FieldViewer::drawButton(float x, float y, float w, float h, const char *label, bool hovered, bool pressed, float textScale){
  float r = hovered ? (pressed ? 0.20f : 0.34f) : 0.24f;
  float g = r, b = r + 0.02f;
  if (pressed && hovered) { r = 0.18f; g = 0.18f; b = 0.20f; }
  drawRect(x, y, w, h, r, g, b, 1.0f);
  if (hovered) {
    drawRectBorder(x, y, w, h, 0.58f, 0.58f, 0.60f);
  } else {
    drawRectBorder(x, y, w, h, 0.38f, 0.38f, 0.40f);
  }
  // Center label with crisp scaled font
  int len = int(std::strlen(label));
  float textW = float(len) * 6.0f * textScale;
  float tx = x + (w - textW) * 0.5f;
  float ty = y + (h - 7.0f * textScale) * 0.5f - 0.5f;
  if (pressed) { tx += 1.0f; ty -= 1.0f; }
  if (hovered) {
    glColor3f(1.0f, 1.0f, 0.98f);
  } else {
    glColor3f(0.90f, 0.90f, 0.88f);
  }
  drawText(tx, ty, label, textScale);
  return hovered;
}

bool FieldViewer::hitTest(float mx, float my, float x, float y, float w, float h){
  return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

bool FieldViewer::drawControlBar(int ww, int wh, double mouseX, double mouseY, bool mousePressed,
                                int particleCount, float speed, bool paused,
                                int batchCount, float intervalSec,
                                bool pulseMode, const char *collModeStr){
  endFieldTransform();
  beginUI(ww, wh);

  // Convert mouseX, mouseY from window coords to exact framebuffer coords
  int winW, winH;
  glfwGetWindowSize(window_, &winW, &winH);
  double scaleX = (winW > 0) ? (double(ww) / double(winW)) : 1.0;
  double scaleY = (winH > 0) ? (double(wh) / double(winH)) : 1.0;
  double fbMx = mouseX * scaleX;
  double fbMy = mouseY * scaleY;

  double uiMx = fbMx;
  double uiMy = double(wh) - fbMy; // bottom origin

  // Dynamic UI scaling based on window/framebuffer resolution
  float baseScale = std::clamp(float(ww) / 1280.0f, 0.55f, 2.5f);
  float barH = std::clamp(std::round(48.0f * baseScale), 28.0f, std::min(120.0f, float(wh) * 0.22f));
  currentBarHeight_ = barH;
  float barY = float(wh) - barH;

  float uiScale = barH / 48.0f; // exact scaling factor tied to bar height

  // Bar background — matte dark neutral
  drawRect(0, barY, float(ww), barH, 0.13f, 0.13f, 0.15f, 0.98f);
  // Bottom border line
  float borderThick = std::max(1.0f, 1.5f * uiScale);
  drawRect(0, barY, float(ww), borderThick, 0.28f, 0.28f, 0.30f, 1.0f);

  // Button sizes and typography scaling
  float h = std::clamp(std::round(34.0f * uiScale), 20.0f, barH - 4.0f);
  float y = barY + (barH - h) * 0.5f;

  float textScale = std::max(0.95f, 1.70f * uiScale);
  float statusScale = std::max(0.90f, 1.50f * uiScale);
  float hintScale = std::max(0.80f, 1.30f * uiScale);

  bool compactMode = (float(ww) < 950.0f * uiScale) || (float(ww) < 900.0f);

  struct Btn { float x, w; std::string label; int id; };
  std::vector<Btn> btns;

  float curX = std::max(6.0f, 10.0f * uiScale);
  float btnSpacing = std::max(3.0f, 6.0f * uiScale);
  float groupGap = std::max(2.0f, 4.0f * uiScale);

  auto addBtn = [&](const std::string &lab, int id, float extraPad = 0.0f) {
    int len = int(lab.length());
    float textW = float(len) * 6.0f * textScale;
    float w = std::max(textW + (14.0f + extraPad) * uiScale, 36.0f * uiScale);
    btns.push_back({curX, w, lab, id});
    curX += w + btnSpacing;
  };

  // 1. Mode toggle: PULSE vs STREAM
  if (compactMode) {
    addBtn(pulseMode ? "PULSE" : "STRM", 0);
  } else {
    addBtn(pulseMode ? "PULSE" : "STREAM", 0);
  }

  // 2. Batch size: BATCH - and BATCH +
  if (compactMode) {
    addBtn("B -", 1);
    addBtn("B +", 2);
  } else {
    addBtn("BATCH -", 1);
    addBtn("BATCH +", 2);
  }
  curX += groupGap;

  // 3. Frequency / Interval: FREQ - and FREQ +
  if (compactMode) {
    addBtn("F -", 3);
    addBtn("F +", 4);
  } else {
    addBtn("FREQ -", 3);
    addBtn("FREQ +", 4);
  }
  curX += groupGap;

  // 4. Flow speed: SPD - and SPD +
  if (compactMode) {
    addBtn("S -", 5);
    addBtn("S +", 6);
  } else {
    addBtn("SPD -", 5);
    addBtn("SPD +", 6);
  }
  curX += groupGap;

  // 5. Emit manual burst
  addBtn("BURST", 7);

  // 6. Reset view & pause/play
  if (compactMode) {
    addBtn("RST", 8);
    addBtn(paused ? "PLAY" : "||", 9);
  } else {
    addBtn("RESET", 8);
    addBtn(paused ? "PLAY" : "PAUSE", 9);
  }

  bool anyHover = false;
  bool uiHover = uiMy >= barY && uiMy <= float(wh);

  for (auto &b: btns){
    bool hover = uiHover && hitTest(float(uiMx), float(uiMy), b.x, y, b.w, h);
    bool pressed = hover && mousePressed;
    if (hover) anyHover = true;
    drawButton(b.x, y, b.w, h, b.label.c_str(), hover, pressed, textScale);
    bool clicked = hover && mousePressed && !uiMouseDownPrev_;
    if (clicked){
      switch(b.id){
        case 0: if(onToggleEmission) onToggleEmission(); break;
        case 1: if(onBatchDelta) onBatchDelta(-50); break;
        case 2: if(onBatchDelta) onBatchDelta(50); break;
        case 3: if(onIntervalDelta) onIntervalDelta(0.05f); break; // longer interval = lower freq
        case 4: if(onIntervalDelta) onIntervalDelta(-0.05f); break; // shorter interval = higher freq
        case 5: if(onSpeedDelta) onSpeedDelta(0.85f); break;
        case 6: if(onSpeedDelta) onSpeedDelta(1.18f); break;
        case 7: if(onBurst) onBurst(); break;
        case 8: resetView(); if(onResetView) onResetView(); break;
        case 9: if(onTogglePause) onTogglePause(); break;
      }
    }
  }

  // Status text: active count, batch size, interval, speed, zoom, mode
  curX += std::max(6.0f, 12.0f * uiScale);
  float remainingW = float(ww) - curX - std::max(6.0f, 10.0f * uiScale);

  char buf[160];
  if (remainingW >= 420.0f * statusScale / 1.5f) {
    if (collModeStr && *collModeStr) {
      std::snprintf(buf, sizeof(buf), "%d PTS  %d/%.2fs  SPD %.2f  Z %.2fX  [%s]",
                    particleCount, batchCount, intervalSec, speed, zoom_, collModeStr);
    } else {
      std::snprintf(buf, sizeof(buf), "%d PTS  %d/%.2fs  SPD %.2f  Z %.2fX",
                    particleCount, batchCount, intervalSec, speed, zoom_);
    }
  } else if (remainingW >= 270.0f * statusScale / 1.5f) {
    std::snprintf(buf, sizeof(buf), "%d PTS  %d/%.2fs  SPD %.2f  Z %.2fX",
                  particleCount, batchCount, intervalSec, speed, zoom_);
  } else if (remainingW >= 140.0f * statusScale / 1.5f) {
    std::snprintf(buf, sizeof(buf), "%d PTS  SPD %.2f", particleCount, speed);
  } else if (remainingW >= 60.0f * statusScale / 1.5f) {
    std::snprintf(buf, sizeof(buf), "%d PTS", particleCount);
  } else {
    buf[0] = '\0';
  }

  if (buf[0]) {
    float statusY = barY + (barH - 7.0f * statusScale) * 0.5f - 0.5f;
    glColor3f(0.80f, 0.80f, 0.78f);
    drawText(curX, statusY, buf, statusScale);
  }

  // Right-aligned helper hint if there is space
  const char *hint = "DRAG PAN   SCROLL ZOOM";
  float hintW = float(std::strlen(hint)) * 6.0f * hintScale;
  float statusW = float(std::strlen(buf)) * 6.0f * statusScale;
  if (float(ww) - hintW - 14.0f * uiScale > curX + statusW + 20.0f * uiScale) {
    float hintY = barY + (barH - 7.0f * hintScale) * 0.5f - 0.5f;
    glColor3f(0.50f, 0.50f, 0.48f);
    drawText(float(ww) - hintW - 14.0f * uiScale, hintY, hint, hintScale);
  }

  uiMouseDownPrev_ = mousePressed;

  endUI();
  beginFieldTransform(ww, wh);
  return uiHover;
}

void FieldViewer::endFrame(){
  endFieldTransform();
  glfwSwapBuffers(window_);
}

void FieldViewer::setTitle(const std::string &t){ glfwSetWindowTitle(window_, t.c_str()); }

} // namespace cpu_lbm
