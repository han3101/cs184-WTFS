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

bool FieldViewer::drawButton(float x,float y,float w,float h,const char *label,bool hovered,bool pressed){
  float r = hovered ? (pressed ? 0.20f : 0.32f) : 0.22f;
  float g = r, b = r + 0.02f;
  if (pressed && hovered) { r=0.18f; g=0.18f; b=0.20f; }
  drawRect(x,y,w,h,r,g,b,1.0f);
  if (hovered) {
    drawRectBorder(x,y,w,h,0.50f,0.50f,0.52f);
  } else {
    drawRectBorder(x,y,w,h,0.34f,0.34f,0.36f);
  }
  // Center label
  int len = int(std::strlen(label));
  float textScale = 1.35f;
  float textW = len * 6 * textScale;
  float tx = x + (w - textW) * 0.5f;
  float ty = y + (h - 7*textScale)*0.5f - 1.0f;
  if (pressed) { tx += 1.0f; ty -= 1.0f; }
  if (hovered) {
    glColor3f(1.0f, 1.0f, 0.98f);
  } else {
    glColor3f(0.88f, 0.88f, 0.86f);
  }
  drawText(tx, ty, label, textScale);
  return hovered;
}

bool FieldViewer::hitTest(float mx,float my,float x,float y,float w,float h){
  return mx>=x && mx<=x+w && my>=y && my<=y+h;
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

  const float barH = controlBarHeight();
  float barY = float(wh) - barH;

  // Bar background — matte dark neutral
  drawRect(0, barY, float(ww), barH, 0.13f, 0.13f, 0.15f, 0.98f);
  // Bottom border line
  drawRect(0, barY, float(ww), 1.0f, 0.26f, 0.26f, 0.28f, 1.0f);

  // Buttons layout
  struct Btn { float x, w; const char* label; int id; };
  float h = 24.0f;
  float y = barY + (barH - h) * 0.5f;

  std::vector<Btn> btns;
  float curX = 8.0f;
  auto add = [&](float w_, const char* lab, int id){
    btns.push_back({curX, w_, lab, id});
    curX += w_ + 5.0f;
  };

  // 1. Mode toggle: PULSE vs STREAM
  add(56, pulseMode ? "PULSE" : "STREAM", 0);

  // 2. Batch size: BATCH - and BATCH + (changes particles per pulse)
  add(54, "BATCH -", 1);
  add(54, "BATCH +", 2);
  curX += 4.0f;

  // 3. Frequency / Interval: FREQ - and FREQ + (changes time between pulses)
  add(52, "FREQ -", 3);
  add(52, "FREQ +", 4);
  curX += 4.0f;

  // 4. Flow speed: SPD - and SPD +
  add(50, "SPD -", 5);
  add(50, "SPD +", 6);
  curX += 4.0f;

  // 5. Emit manual burst
  add(50, "BURST", 7);

  // 6. Reset view & pause/play
  add(50, "RESET", 8);
  add(52, paused ? "PLAY" : "PAUSE", 9);

  bool anyHover = false;
  bool uiHover = uiMy >= barY && uiMy <= float(wh);

  for (auto &b: btns){
    bool hover = uiHover && hitTest(float(uiMx), float(uiMy), b.x, y, b.w, h);
    bool pressed = hover && mousePressed;
    if (hover) anyHover = true;
    drawButton(b.x, y, b.w, h, b.label, hover, pressed);
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
  curX += 10.0f;
  char buf[160];
  if (collModeStr && *collModeStr) {
    std::snprintf(buf, sizeof(buf), "%d PTS  %d/%.2fs  SPD %.2f  Z %.2fX  [%s]",
                  particleCount, batchCount, intervalSec, speed, zoom_, collModeStr);
  } else {
    std::snprintf(buf, sizeof(buf), "%d PTS  %d/%.2fs  SPD %.2f  Z %.2fX",
                  particleCount, batchCount, intervalSec, speed, zoom_);
  }
  glColor3f(0.76f, 0.76f, 0.74f);
  drawText(curX, y + 4.0f, buf, 1.35f);

  // Right-aligned helper hint if there is space
  const char *hint = "DRAG PAN   SCROLL ZOOM";
  float hintW = float(std::strlen(hint)) * 6.0f * 1.15f;
  if (float(ww) - hintW - 12.0f > curX + 260.0f) {
    glColor3f(0.46f, 0.46f, 0.44f);
    drawText(float(ww) - hintW - 12.0f, y + 5.0f, hint, 1.15f);
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
