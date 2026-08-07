#ifndef CLOTH_H
#define CLOTH_H

#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "CGL/CGL.h"
#include "CGL/misc.h"
#include "clothMesh.h"
#include "collision/collisionObject.h"
#include "spring.h"

using namespace CGL;
using namespace std;

enum e_orientation { HORIZONTAL = 0, VERTICAL = 1 };

struct ClothParameters {
  ClothParameters() {}
  ClothParameters(bool enable_structural_constraints,
                  bool enable_shearing_constraints,
                  bool enable_bending_constraints, double damping,
                  double density, double ks)
      : enable_structural_constraints(enable_structural_constraints),
        enable_shearing_constraints(enable_shearing_constraints),
        enable_bending_constraints(enable_bending_constraints),
        damping(damping), density(density), ks(ks) {}
  ~ClothParameters() {}

  // Global simulation parameters

  bool enable_structural_constraints;
  bool enable_shearing_constraints;
  bool enable_bending_constraints;

  double damping;

  // Mass-spring parameters
  double density;
  double ks;
};

struct Cloth {
  Cloth() {}
  Cloth(double width, double height, int num_width_points,
        int num_height_points, float thickness);
  ~Cloth();

  void buildGrid();
  void addGrid();

  void simulate(double frames_per_sec, double simulation_steps, ClothParameters *cp,
                vector<Vector3D> external_accelerations,
                vector<CollisionObject *> *collision_objects);

  void reset();
  //void buildClothMesh();

  // For Windsim, control settings
  double h = 0.12; // h is the distance cap on nearest neighbors
  double c = 0.01; // c is some constant used in applying XSPH viscosity (pg 3 of Macklin and Muller)
  double k = 0.00001; // k is a small positive constant used in calculating an artificial pressure (pg 3 of Macklin and Muller)
  double n = 4; // n is some constant used in calculating an artificial pressure (pg 3 of Macklin and Muller)
  Vector3D delta_q = 0.2 * h * Vector3D(1, 0, 0); // delta_q is a point some fixed distance inside the smoothing kernel radius (pg 3 of Macklin and Muller)
  double relaxation = 160000; // eps
  double vorticity_eps = 0.0002;
  void set_neighbors(PointMass &pm, double h);
  int hash_box(Vector3D pos, double h);
//  void decode_position(int key, int &x_box, int &y_box, int &z_box);

  void build_spatial_map();
  void self_collide(PointMass &pm, double simulation_steps);
  float hash_position(Vector3D pos);

  // Navier-stokes methods

  void calculate_lambda(PointMass &pm, double mass, double density, double h, double relaxation);

  void calculate_delta_p(PointMass &pm, double h, Vector3D delta_q, double k, double n, double density);

  // c is some constant used in applying XSPH viscosity (see page 3 of Macklin and Muller)
  void viscosity(PointMass &pm, double c, double h);

  void calculate_omega(PointMass &pm, double h);

  void vorticity(PointMass &pm, double h, double delta_t, double vorticity_eps, double mass);


  // Navier-stokes helper
  double poly6_kernel(Vector3D r, double h) {
      if (r.norm() > h) {
          return 0;
      }
      return 315.0 / (64 * PI * pow(h, 9)) * pow(pow(h, 2) - r.norm2(), 3);
  }

    Vector3D spiky_kernel(Vector3D r, double h) {
        double r_len = r.norm();
        if (r_len > h || r_len < 1e-9) {
            return {0,0,0};
        }
        return -r * 45.0 / (PI * pow(h, 6) * r_len) * pow(h - r_len, 2);
    }

  // Cloth properties
  double width;
  double height;
  int num_width_points;
  int num_height_points;
  double thickness;
  e_orientation orientation;

  // Cloth components
  vector<PointMass> point_masses;
  vector<vector<int>> pinned;
  vector<Spring> springs;
  ClothMesh *clothMesh;

  // Spatial hashing
  unordered_map<int, vector<PointMass *> *> map;
};

#endif /* CLOTH_H */
