#define GL_SILENCE_DEPRECATION
#include "field.h"
#include "units.h"
#include "lbm.h"
#include "boundary.h"
#include "obstacle.h"
#include "tracers.h"
#include "field_viewer.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

static void print_usage(const char* prog){
  std::fprintf(stderr,
    "Usage: %s [options]\n"
    " cyl2d_live — LBM-driven viewer (D2Q9) with tracers + obstacles\n"
    " Options:\n"
    "  --nx N --ny N     grid (default 400x80)\n"
    "  --re F            Reynolds (default 100)\n"
    "  --u0 F            inlet velocity (default 0.05)\n"
    "  --steps N         0=infinite\n"
    "  --scale N         window scale\n"
    "  --uniform         uniform fallback (no LBM)\n"
    "  --circle cx cy r / --block cx cy w h / --no-obstacle\n",
    prog);
}

int main(int argc, char** argv){
  int nx=400, ny=80;
  float Re=100; float u0=0.05f;
  int scale=3; bool uniform=false;
  std::vector<cpu_lbm::Obstacle> obstacles;
  bool noDefault=false;
  for(int i=1;i<argc;++i){
    std::string a=argv[i];
    auto need=[&](const char* n){ if(i+1>=argc){ std::fprintf(stderr,"missing %s\n",n); print_usage(argv[0]); std::exit(1);} };
    if(a=="--nx"&&(need("--nx"),true)) nx=std::atoi(argv[++i]);
    else if(a=="--ny"&&(need("--ny"),true)) ny=std::atoi(argv[++i]);
    else if(a=="--re"&&(need("--re"),true)) Re=float(std::atof(argv[++i]));
    else if(a=="--u0"&&(need("--u0"),true)) u0=float(std::atof(argv[++i]));
    else if(a=="--scale"&&(need("--scale"),true)) scale=std::atoi(argv[++i]);
    else if(a=="--uniform") uniform=true;
    else if(a=="--circle"&&(need("--circle"),true)){ float cx=float(atof(argv[++i])), cy=float(atof(argv[++i])), r=float(atof(argv[++i])); obstacles.push_back(cpu_lbm::Obstacle::makeCircle(cx,cy,r)); }
    else if(a=="--block"&&(need("--block"),true)){ float cx=float(atof(argv[++i])), cy=float(atof(argv[++i])), w=float(atof(argv[++i])), h=float(atof(argv[++i])); obstacles.push_back(cpu_lbm::Obstacle::makeBlock(cx,cy,w,h)); }
    else if(a=="--no-obstacle"||a=="--no-obstacles") noDefault=true;
    else if(a=="--help"||a=="-h"){ print_usage(argv[0]); return 0; }
  }
  if(obstacles.empty() && !noDefault){
    float D=float(ny)*0.25f;
    obstacles.push_back(cpu_lbm::Obstacle::makeCircle(float(nx)*0.30f, float(ny)*0.5f, D*0.5f));
  }
  float D = float(ny)*0.25f;
  if(!obstacles.empty() && obstacles[0].type==cpu_lbm::Obstacle::CIRCLE) D=obstacles[0].r*2.0f;

  cpu_lbm::Field2D field(nx,ny);
  field.initEquilibrium(u0,0.0f,1.0f);
  if(!uniform) cpu_lbm::boundary::buildSolidMask(field, obstacles);
  auto units = cpu_lbm::LbUnits::fromRe(Re, D, u0);
  if(!uniform && !units.stable())
    std::fprintf(stderr,"WARN tau=%.4f unstable for Re=%.1f D=%.1f — increase resolution\n", units.tau, Re, D);
  const float tau = units.tau;

  cpu_lbm::TracerSet tracers(0,nx,ny,42);
  tracers.setEmitConfig(300,0.30f,true);
  tracers.emitBurst(300);

  cpu_lbm::FieldViewer viewer(nx,ny,scale,"WTFS — cyl2d_live (LBM)");
  auto* win=viewer.window();
  bool paused=false;
  while(!viewer.shouldClose()){
    viewer.pollEvents();
    if(glfwGetKey(win, GLFW_KEY_SPACE)==GLFW_PRESS){ static bool prev=false; bool now=true; if(now&&!prev) paused=!paused; prev=now; }
    if(glfwGetKey(win, GLFW_KEY_ESCAPE)==GLFW_PRESS) break;
    if(!paused){
      if(!uniform){
        cpu_lbm::collideAndStream(field, tau);
        cpu_lbm::boundary::applyAll(field, u0);
        field.macroscopic();
      }
      tracers.stepEmission(1.0f/60.0f);
      tracers.advect(field, 1.0f, obstacles, cpu_lbm::CollMode::Passive);
    }
    viewer.beginFrame();
    viewer.drawField(field);
    viewer.drawObstacles(obstacles);
    viewer.drawTracers(tracers);
    viewer.endFrame();
  }
  return 0;
}
