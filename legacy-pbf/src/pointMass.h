#ifndef POINTMASS_H
#define POINTMASS_H

#include "CGL/CGL.h"
#include "CGL/misc.h"
#include "CGL/vector3D.h"
#include "misc/sphere_drawing.h"

using namespace CGL;

// Forward declarations
class Halfedge;
static float GAS_K = 0.1f;
static float DENSITY_OFFSET = 100;

struct PointMass {
  PointMass(Vector3D position, Vector3D color)
      : color(color), start_position(position), position(position),
        last_position(position), last_velocity(Vector3D(0, 0, 0.01)), temp_velocity(Vector3D()),
        delta_p(Vector3D()), neighbors(new std::vector<PointMass*>()), omega(Vector3D()){}

  Vector3D normal();
  Vector3D velocity(double delta_t) {
    return (position - last_position) / delta_t;
  }

  void updatePressure() {
      this->pressure = GAS_K * (this->density - DENSITY_OFFSET);
  }

  Vector3D start_position;

  // dynamic values
  Vector3D position;
  Vector3D last_position;
  Vector3D forces;
  Vector3D color;
  double density;
  double pressure;

  // Values for navier-stokes-sph
  Vector3D last_velocity;
  Vector3D temp_velocity;
  Vector3D delta_p;

  std::vector<PointMass*> *neighbors;
  double lambda;
  double rho;
  Vector3D omega;
//  Vector3D normal;

  // mesh reference
  Halfedge *halfedge;

  // Draw pointmasses as spheres
  Misc::SphereMesh m_sphere_mesh;
};

#endif /* POINTMASS_H */
