#include "iostream"
#include <nanogui/nanogui.h>

#include "../clothMesh.h"
#include "../clothSimulator.h"
#include "plane.h"

using namespace std;
using namespace CGL;

#define SURFACE_OFFSET 0.0001

//double get_t(Vector3D pos, Vector3D normal, Vector3D point) {
//    return dot(point - pos, normal) / dot(normal, normal);
//}

void Plane::collide(PointMass &pm, double delta_t) {
  // TODO (Part 3): Handle collisions with planes.
    
    if (dot(point - pm.position, normal) * dot(point - pm.last_position, normal) > 0.0) {
        return;
    } 
//    else if (dot(point - pm.position, normal) == 0.0 && dot(point - pm.last_position, normal) != 0.0) {
//        return;
//    } else if (dot(point - pm.position, normal) != 0.0 && dot(point - pm.last_position, normal) == 0.0) {
//        return;
//    }
    
    double t = dot(point - pm.position, normal) / dot(normal, normal);
    
    Vector3D tangentPoint = pm.position + t*normal;
    
    Vector3D correction = tangentPoint - pm.last_position + normal * SURFACE_OFFSET;

    pm.position = pm.last_position + correction * (1.0-friction);
    
    //new code
//    Vector3D dir = pm.position-origin;
//    
//    if (dir.norm() > radius) {
//        pm.last_velocity = pm.velocity(delta_t);
//        return;
//    }

//    Vector3D normal = dir.unit();  // Normal vector from origin to the point mass.
//    Vector3D tangentPoint = origin + normal * radius;  // Calculate tangent point on the sphere surface.

//    Vector3D correction = tangentPoint - pm.position;  // Correction to move to the sphere's surface.
    Vector3D velocity = (pm.position - pm.last_position) / delta_t;  // Calculate current velocity of the point mass

    // Decompose the velocity into normal and tangential components
    double velocityNormalComponent = dot(velocity, normal);  // Normal component of velocity (scalar)
    Vector3D velocityNormal = normal * velocityNormalComponent;  // Normal component of velocity (vector)
    Vector3D velocityTangent = velocity - velocityNormal;  // Tangential component of velocity

    // Reflect the normal component for the bounce (perfectly elastic collision)
    // Coefficient of Restitution (cr) is typically between 0 and 1, with 1 being a perfectly elastic collision.
    double cr = 1.0; // Assuming a perfectly elastic collision
    Vector3D reflectedVelocityNormal = velocityNormal * (-cr);

    // Apply the correction to the point mass's position
    pm.position = tangentPoint + reflectedVelocityNormal * delta_t + velocityTangent * delta_t * (1 - friction);
    pm.last_position = tangentPoint;  // Update the last position to the tangent point post-collision
    pm.last_velocity = reflectedVelocityNormal + velocityTangent * (1-friction);
    pm.temp_velocity = reflectedVelocityNormal + velocityTangent * (1-friction);

}

void Plane::render(GLShader &shader) {
  nanogui::Color color(0.7f, 0.7f, 0.7f, 1.0f);

  Vector3f sPoint(point.x, point.y, point.z);
  Vector3f sNormal(normal.x, normal.y, normal.z);
  Vector3f sParallel(normal.y - normal.z, normal.z - normal.x,
                     normal.x - normal.y);
  sParallel.normalize();
  Vector3f sCross = sNormal.cross(sParallel);

  MatrixXf positions(3, 4);
  MatrixXf normals(3, 4);

  positions.col(0) << sPoint + 2 * (sCross + sParallel);
  positions.col(1) << sPoint + 2 * (sCross - sParallel);
  positions.col(2) << sPoint + 2 * (-sCross + sParallel);
  positions.col(3) << sPoint + 2 * (-sCross - sParallel);

  normals.col(0) << sNormal;
  normals.col(1) << sNormal;
  normals.col(2) << sNormal;
  normals.col(3) << sNormal;

  if (shader.uniform("u_color", false) != -1) {
    shader.setUniform("u_color", color);
  }
  shader.uploadAttrib("in_position", positions);
  if (shader.attrib("in_normal", false) != -1) {
    shader.uploadAttrib("in_normal", normals);
  }

  shader.setUniform("is_plane", true, false);
  shader.drawArray(GL_TRIANGLE_STRIP, 0, 4);
  shader.setUniform("is_plane", false, false);
}
