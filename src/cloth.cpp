#include <iostream>
#include <math.h>
#include <random>
#include <vector>

#include "cloth.h"
#include "collision/plane.h"
#include "collision/sphere.h"

using namespace std;

Cloth::Cloth(double width, double height, int num_width_points,
             int num_height_points, float thickness) {
  this->width = width;
  this->height = height;
  this->num_width_points = num_width_points;
  this->num_height_points = num_height_points;
  this->thickness = thickness;
  buildGrid();
  //buildClothMesh();
}

Cloth::~Cloth() {
  point_masses.clear();
  springs.clear();

  if (clothMesh) {
    delete clothMesh;
  }
}

bool checkPinned(Vector3D pos, vector<vector<int>> pinned) {
    if (!(pos.x >= pinned[0][0] && pos.x <= pinned[1][0])) {
        return false;
    } else if (!(pos.y >= pinned[0][1] && pos.y <= pinned[1][1])) {
        return false;
    }
    return true;
}

int getPMAbove(Vector3D pos) {
    
}

void Cloth::buildGrid() {
  // TODO (Part 1): Build a grid of masses and springs.
    
    if (num_width_points == 0 || num_height_points == 0) {
        return;
    }

    double width_diff = width/(double) (num_width_points - 1);
    double height_diff = height/(double) (num_height_points - 1);
    double width_start = width_diff/2;
    double height_start = height_diff/2;

    //Create an evenly spaced grid of masses
    if (orientation == HORIZONTAL) {
        for (int i=0; i<num_height_points; ++i) {
            for (int j=0; j<num_width_points; ++j) {
                Vector3D pos = Vector3D(width_start+j*width_diff, 1, height_start+i*height_diff);                
                PointMass pm = PointMass(pos, Vector3D(0, 0, 0));
                point_masses.emplace_back(pm);
            }
        }
    } else if (orientation == VERTICAL) {
        double min_off = -1.0 / 1000;
        double max_off = 1.0 / 1000;
        double y = 0;
        for (int j = 0; j < num_height_points; ++j) {
            double x = 0;
            for (int i = 0; i < num_width_points; ++i) {
                double rand_off = (double) rand() / RAND_MAX - 1;
                double z = min_off + rand_off * (max_off - min_off);
                point_masses.emplace_back(Vector3D(x, y, z), false);
                x += width_diff;
            }
            y += height_diff;
        }
    }
}

void Cloth::addGrid() {
    if (num_width_points == 0 || num_height_points == 0) {
        return;
    }

    double width_diff = width/(double) (num_width_points - 1);
    double height_diff = height/(double) (num_height_points - 1);
    double width_start = width_diff/2;
    double height_start = height_diff/2;

    //Create an evenly spaced grid of masses
    if (orientation == HORIZONTAL) {
        for (int i=0; i<num_height_points; ++i) {
            for (int j=0; j<num_width_points; ++j) {
                Vector3D pos = Vector3D(width_start+j*width_diff, 1, height_start+i*height_diff);                
                PointMass pm = PointMass(pos, Vector3D(0, 0, 0));
                point_masses.emplace_back(pm);
            }
        }
    } else if (orientation == VERTICAL) {
        double min_off = -1.0 / 1000;
        double max_off = 1.0 / 1000;
        double y = 0;
        for (int j = 0; j < num_height_points; ++j) {
            double x = 0;
            for (int i = 0; i < num_width_points; ++i) {
                double rand_off = (double) rand() / RAND_MAX - 1;
                double z = min_off + rand_off * (max_off - min_off);
                point_masses.emplace_back(Vector3D(x, y, z), false);
                x += width_diff;
            }
            y += height_diff;
        }
    }
}

void Cloth::simulate(double frames_per_sec, double simulation_steps, ClothParameters *cp,
                     vector<Vector3D> external_accelerations,
                     vector<CollisionObject *> *collision_objects) {
  double mass = width * height * cp->density / num_width_points / num_height_points;
  double delta_t = 1.0f / frames_per_sec / simulation_steps;

  // TODO (Part 2): Compute total force acting on each point mass.
  Vector3D total_external_forces;
  for(const Vector3D& acc: external_accelerations)
      total_external_forces += acc;


  // Update velocity and position by applying forces
  total_external_forces *= mass;
  for(PointMass &pm: this->point_masses) {
      pm.forces = total_external_forces;
//      Vector3D velocity = pm.last_velocity;
//      velocity += delta_t * pm.forces / mass;
//      cout << "last_velocity" << endl;
//      cout << pm.last_velocity << endl;
//      cout << "velocity" << endl;
//      cout << velocity << endl;
//      Vector3D curr_pos = pm.position;
//      pm.position = curr_pos + delta_t * velocity;
//      pm.last_position = curr_pos;
  }

//  // TODO (Part 2): Use Verlet integration to compute new point mass positions
  for (PointMass &pm : point_masses) {
      Vector3D curr_pos = pm.position;
      Vector3D last_pos = pm.last_position;
      Vector3D accel = pm.forces / mass;
      pm.position = curr_pos + (double)(1.0f - cp->damping / 100.0f) * (curr_pos - last_pos) + accel * pow(delta_t, 2);
      pm.last_position = curr_pos;
  }


  // Find and update neighboring particles
  build_spatial_map();
//#pragma omp parallel for
  for (PointMass &pm : point_masses) {
      set_neighbors(pm, h);
  }

  for (int i = 0; i < 2; i++) {
      for (PointMass &pm : point_masses) {
          calculate_lambda(pm, mass, cp->density,h, relaxation);
      }

      for (PointMass &pm : point_masses) {
          calculate_delta_p(pm, h, delta_q, k, n, cp->density);
      }

      for (PointMass &pm : point_masses) {
          Vector3D force_f = pm.delta_p * mass / delta_t;
          pm.position += pm.delta_p;
//          cout << pm.delta_p << endl;
//          cout << "LOL" << endl;
      }
  }

//  for (PointMass &pm : point_masses) {
//      viscosity(pm, c, h);
//  }
//
//  for (PointMass &pm : point_masses) {
//      calculate_omega(pm, h);
//  }
//
//  for (PointMass &pm : point_masses) {
//      vorticity(pm, h, delta_t, vorticity_eps, mass);
//  }



    // TODO (Part 4): Handle self-collisions.
//  for (PointMass &pm : point_masses) {
//      self_collide(pm, simulation_steps);
//  }


//   TODO (Part 3): Handle collisions with other primitives.
    for (PointMass &pm : point_masses) {
        for (CollisionObject *co : *collision_objects) {
            co->collide(pm, delta_t);
        }
    }

// Update velocity
//for (PointMass &pm : point_masses) {
//    pm.last_velocity = pm.temp_velocity;
////    cout << pm.temp_velocity << endl;
//}


    // TODO (Part 2): Constrain the changes to be such that the spring does not change
  // in length more than 10% per timestep [Provot 1995].

//DELETE Points outside boundary
    std::vector<PointMass*> to_delete;
    for (PointMass& pm : point_masses) {
        if (pm.position.x > 5 || pm.position.y > 5 || pm.position.z > 5) {
            to_delete.push_back(&pm);
        }
    }
  point_masses.erase(std::remove_if(point_masses.begin(), point_masses.end(), [&to_delete](const PointMass& pm) {return std::find(to_delete.begin(), to_delete.end(), &pm) != to_delete.end();}), point_masses.end());

}

void Cloth::build_spatial_map() {
  for (const auto &entry : map) {
    delete(entry.second);
  }
  map.clear();

  // TODO (Part 4): Build a spatial map out of all of the point masses.
  for (PointMass &pm : point_masses) {
      int hash = hash_box(pm.position, h);
      if (!map[hash]) {
          map[hash] = new vector<PointMass*>;
      }
      map[hash]->push_back(&pm);
  }

}

void Cloth::self_collide(PointMass &pm, double simulation_steps) {
  // NOTE: legacy cloth self-collision — not used in PBF path (use hash_box).
  // Kept for compatibility; now delegates to hash_box int key.
    int hash = hash_box(pm.position, h);
    Vector3D correction;
    int count = 0;
    auto it = map.find(hash);
    if(it != map.end())
    {
        for(PointMass *pm_iter: *(it->second))
        {
            Vector3D diff = pm.position - pm_iter->position;
            if(pm_iter!=&pm && diff.norm() <= 1 * thickness)
            {
                correction += diff.unit() * (1 * thickness - diff.norm());
                count++;
            }
        }
    }
    if(count)
        pm.position += correction / count / simulation_steps;

}

float Cloth::hash_position(Vector3D pos) {
  // TODO (Part 4): Hash a 3D position into a unique float identifier that represents membership in some 3D box volume.
  double w = 3.0 * width / (num_width_points - 1);
  double h = 3.0 * height / (num_height_points - 1);
  double t = max(w, h);

  float x = floor(pos.x / w);
  float y = floor(pos.y / h);
  float z = floor(pos.z / t);

  float hash = (x * 31 + y) * 31 + z;

  return hash;

}

// Hashes Vector3D position into a single integer that belongs to a 3D box
// to check if neighboring particles are within the box or adjacent boxes
int Cloth::hash_box(CGL::Vector3D pos, double h) {
    int x_box = floor(pos.x / h);
    int y_box = floor(pos.y / h);
    int z_box = floor(pos.z / h);

    int key = (x_box & 0x3FF) | ((y_box & 0x3FF) << 10) | ((z_box & 0x3FF) << 20);
    return key;

}

void Cloth::set_neighbors(PointMass &pm, double h) {
    Vector3D pos = pm.position;

    (pm.neighbors)->clear();

    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            for (int k = -1; k <= 1; k++) {
                Vector3D offset = Vector3D(i * h, j * h, k * h);
                int neighbor_key = hash_box(pos + offset, h);
                auto it = map.find(neighbor_key);
                if (it != map.end()) {
                    // Iterate over each PointMass in the neighboring cell
                    for (auto q = begin(*(it->second)); q != end(*(it->second)); q++) {
                        double dist = (pm.position - (*q)->position).norm();
                        if (dist <= h && dist > 1e-9) {
                            // avoid duplicate inserts when cells alias (negative coords)
                            bool already = false;
                            for (auto existing : *(pm.neighbors)) {
                                if (existing == *q) { already = true; break; }
                            }
                            if (!already) pm.neighbors->emplace_back(*q);
                        }
                    }
                }
            }
        }
    }
}


// Calculate p->lambda as given in Equation 11
void Cloth::calculate_lambda(PointMass &pm, double mass, double density, double h, double relaxation) {

    if (pm.neighbors->empty()) {
        pm.lambda = 0;
        return;
    }
    // Calculate C_i
    double p_i = 0;
    for (auto q = begin(*(pm.neighbors)); q != end(*(pm.neighbors)); q++) {
        p_i += poly6_kernel(pm.position - (*q)->position, h);
    }
    p_i *= mass;

    double C_i = p_i / density - 1;

    Vector3D eqn1 = Vector3D();
    double eqn2 = 0;
    for (auto pj = begin(*(pm.neighbors)); pj != end(*(pm.neighbors)); pj++) {
        Vector3D r = pm.position - (*pj)->position;
        Vector3D grad_j = spiky_kernel(r, h);

        eqn1 += grad_j;
        eqn2 += grad_j.norm2();
    }
    double denom = 1.0 / pow(density, 2) * (eqn1.norm2() + eqn2);

    pm.lambda = -C_i / (denom + relaxation);
}

// Calculate p->delta_p as given in Equation 14
// Tensile Instability
void Cloth::calculate_delta_p(PointMass &pm, double h, CGL::Vector3D delta_q, double k, double n, double density) {

    Vector3D delta_p = Vector3D();
    for (auto pj= begin(*(pm.neighbors)); pj != end(*(pm.neighbors)); pj++) {
        Vector3D r = pm.position - (*pj)->position;

        double numer = poly6_kernel(r, h);
        double denom = poly6_kernel(delta_q, h);
        double s_corr = -k * pow(numer / denom, n);
        Vector3D grad_j = spiky_kernel(r, h);

        delta_p += (pm.lambda + (*pj)->lambda + s_corr) * grad_j;
    }
    delta_p *= 1.0 / density;
    pm.delta_p = delta_p;
}

// Update p->temp_velocity with viscosity as given in Equation 17
void Cloth::viscosity(PointMass &pm, double c, double h) {
    for (auto pj = begin(*pm.neighbors); pj != end(*pm.neighbors); pj++) {
        Vector3D v_ij = (*pj)->last_velocity - pm.last_velocity;
        pm.temp_velocity += c * v_ij * poly6_kernel(pm.position - (*pj)->position, h);
    }
}

// Vorticity, calculate p->omega as given in Equation 15
void Cloth::calculate_omega(PointMass &pm, double h) {
    pm.omega = Vector3D();

    for (auto pj = begin(*pm.neighbors); pj != end(*pm.neighbors); pj++) {
        Vector3D v_ij = (*pj)->last_velocity - pm.last_velocity;
        pm.omega += cross(v_ij, -spiky_kernel(pm.position - (*pj)->position, h));
    }
}

// Calculate vorticity force on p as given in Equation 16 and apply it to p->temp_velocity
void Cloth::vorticity(PointMass &pm, double h, double delta_t, double vorticity_eps, double mass) {

    Vector3D N = Vector3D();

    if (pm.omega.norm() > EPS_F) {
        for (auto pj = begin(*pm.neighbors); pj != end(*pm.neighbors); pj++) {
            Vector3D r = (*pj)->position - pm.position;
            if (r.norm() <= h) {
                double d_omega = (*pj)->omega.norm() - pm.omega.norm();
                N += Vector3D(d_omega / r.x, d_omega / r.y, d_omega / r.z);
            }
        }

        if (N.norm() > EPS_F) {
            N.normalize();
        }
    }

    pm.temp_velocity += delta_t * vorticity_eps * cross(N, pm.omega) / mass;
}


///////////////////////////////////////////////////////
/// YOU DO NOT NEED TO REFER TO ANY CODE BELOW THIS ///
///////////////////////////////////////////////////////

void Cloth::reset() {
//  PointMass *pm = &point_masses[0];
//  for (int i = 0; i < point_masses.size(); i++) {
//    pm->position = pm->start_position;
//    pm->last_position = pm->start_position;
//    pm++;
//  }
    point_masses.clear();
}
/*
void Cloth::buildClothMesh() {
  if (point_masses.size() == 0) return;
  return;
  ClothMesh *clothMesh = new ClothMesh();
  vector<Triangle *> triangles;

  // Create vector of triangles
  for (int y = 0; y < num_height_points - 1; y++) {
    for (int x = 0; x < num_width_points - 1; x++) {
      PointMass *pm = &point_masses[y * num_width_points + x];
      // Get neighboring point masses:
      /*                      *
       * pm_A -------- pm_B   *
       *             /        *
       *  |         /   |     *
       *  |        /    |     *
       *  |       /     |     *
       *  |      /      |     *
       *  |     /       |     *
       *  |    /        |     *
       *      /               *
       * pm_C -------- pm_D   *
       *                      *
       MAKE END COMMENT
      
      float u_min = x;
      u_min /= num_width_points - 1;
      float u_max = x + 1;
      u_max /= num_width_points - 1;
      float v_min = y;
      v_min /= num_height_points - 1;
      float v_max = y + 1;
      v_max /= num_height_points - 1;
      
      PointMass *pm_A = pm                       ;
      PointMass *pm_B = pm                    + 1;
      PointMass *pm_C = pm + num_width_points    ;
      PointMass *pm_D = pm + num_width_points + 1;
      
      Vector3D uv_A = Vector3D(u_min, v_min, 0);
      Vector3D uv_B = Vector3D(u_max, v_min, 0);
      Vector3D uv_C = Vector3D(u_min, v_max, 0);
      Vector3D uv_D = Vector3D(u_max, v_max, 0);
      
      
      // Both triangles defined by vertices in counter-clockwise orientation
      triangles.push_back(new Triangle(pm_A, pm_C, pm_B, 
                                       uv_A, uv_C, uv_B));
      triangles.push_back(new Triangle(pm_B, pm_C, pm_D, 
                                       uv_B, uv_C, uv_D));
    }
  }

  // For each triangle in row-order, create 3 edges and 3 internal halfedges
  for (int i = 0; i < triangles.size(); i++) {
    Triangle *t = triangles[i];

    // Allocate new halfedges on heap
    Halfedge *h1 = new Halfedge();
    Halfedge *h2 = new Halfedge();
    Halfedge *h3 = new Halfedge();

    // Allocate new edges on heap
    Edge *e1 = new Edge();
    Edge *e2 = new Edge();
    Edge *e3 = new Edge();

    // Assign a halfedge pointer to the triangle
    t->halfedge = h1;

    // Assign halfedge pointers to point masses
    t->pm1->halfedge = h1;
    t->pm2->halfedge = h2;
    t->pm3->halfedge = h3;

    // Update all halfedge pointers
    h1->edge = e1;
    h1->next = h2;
    h1->pm = t->pm1;
    h1->triangle = t;

    h2->edge = e2;
    h2->next = h3;
    h2->pm = t->pm2;
    h2->triangle = t;

    h3->edge = e3;
    h3->next = h1;
    h3->pm = t->pm3;
    h3->triangle = t;
  }

  // Go back through the cloth mesh and link triangles together using halfedge
  // twin pointers

  // Convenient variables for math
  int num_height_tris = (num_height_points - 1) * 2;
  int num_width_tris = (num_width_points - 1) * 2;

  bool topLeft = true;
  for (int i = 0; i < triangles.size(); i++) {
    Triangle *t = triangles[i];

    if (topLeft) {
      // Get left triangle, if it exists
      if (i % num_width_tris != 0) { // Not a left-most triangle
        Triangle *temp = triangles[i - 1];
        t->pm1->halfedge->twin = temp->pm3->halfedge;
      } else {
        t->pm1->halfedge->twin = nullptr;
      }

      // Get triangle above, if it exists
      if (i >= num_width_tris) { // Not a top-most triangle
        Triangle *temp = triangles[i - num_width_tris + 1];
        t->pm3->halfedge->twin = temp->pm2->halfedge;
      } else {
        t->pm3->halfedge->twin = nullptr;
      }

      // Get triangle to bottom right; guaranteed to exist
      Triangle *temp = triangles[i + 1];
      t->pm2->halfedge->twin = temp->pm1->halfedge;
    } else {
      // Get right triangle, if it exists
      if (i % num_width_tris != num_width_tris - 1) { // Not a right-most triangle
        Triangle *temp = triangles[i + 1];
        t->pm3->halfedge->twin = temp->pm1->halfedge;
      } else {
        t->pm3->halfedge->twin = nullptr;
      }

      // Get triangle below, if it exists
      if (i + num_width_tris - 1 < 1.0f * num_width_tris * num_height_tris / 2.0f) { // Not a bottom-most triangle
        Triangle *temp = triangles[i + num_width_tris - 1];
        t->pm2->halfedge->twin = temp->pm3->halfedge;
      } else {
        t->pm2->halfedge->twin = nullptr;
      }

      // Get triangle to top left; guaranteed to exist
      Triangle *temp = triangles[i - 1];
      t->pm1->halfedge->twin = temp->pm2->halfedge;
    }

    topLeft = !topLeft;
  }

  clothMesh->triangles = triangles;
  this->clothMesh = clothMesh;
}*/
