#include <nanogui/nanogui.h>

#include "../clothMesh.h"
#include "../misc/sphere_drawing.h"
#include "sphere.h"

using namespace nanogui;
using namespace CGL;

void Sphere::collide(PointMass &pm, double delta_t) {
  // TODO (Part 3): Handle collisions with spheres.
    Vector3D dir = pm.position-origin;
    
    if (dir.norm() > radius) {
        pm.last_velocity = pm.velocity(delta_t);
        return;
    }

    Vector3D normal = dir.unit();  // Normal vector from origin to the point mass.
    Vector3D tangentPoint = origin + normal * radius;  // Calculate tangent point on the sphere surface.

    Vector3D correction = tangentPoint - pm.position;  // Correction to move to the sphere's surface.
    Vector3D velocity = (pm.position - pm.last_position) / delta_t;  // Calculate current velocity of the point mass

    // Decompose the velocity into normal and tangential components
    double velocityNormalComponent = dot(velocity, normal);  // Normal component of velocity (scalar)
    Vector3D velocityNormal = normal * velocityNormalComponent;  // Normal component of velocity (vector)
    Vector3D velocityTangent = velocity - velocityNormal;  // Tangential component of velocity

    // Reflect the normal component for the bounce (perfectly elastic collision)
    // Coefficient of Restitution (cr) is typically between 0 and 1, with 1 being a perfectly elastic collision.
    double cr = 0.45; // Assuming a perfectly elastic collision
    Vector3D reflectedVelocityNormal = velocityNormal * (-cr);

    // Apply the correction to the point mass's position
    pm.position = tangentPoint + reflectedVelocityNormal * delta_t + velocityTangent * delta_t * (1 - friction);
    pm.last_position = tangentPoint;  // Update the last position to the tangent point post-collision
//    pm.temp_velocity = reflectedVelocityNormal + velocityTangent * (1-friction);


}

void Sphere::render(GLShader &shader) {
  // We decrease the radius here so flat triangles don't behave strangely
  // and intersect with the sphere when rendered
  m_sphere_mesh.draw_sphere(shader, origin, radius * 0.92);
}
