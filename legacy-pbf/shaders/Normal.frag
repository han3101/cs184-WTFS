#version 330

in vec4 v_position;
in vec4 v_normal;
in vec4 v_tangent;
in vec3 v_velocity;

out vec4 out_color;

void main() {
  /*
  out_color = (vec4(1, 1, 1, 0) + v_normal) / 2;
  out_color.a = 1;*/

  float speed = length(v_velocity); // Calculate the speed (magnitude of velocity)
  vec3 color = vec3(0.0); // Initialize color as black

  float estimatedMaxVelocity = 1.5;

  color.r = clamp(speed / estimatedMaxVelocity, 0.0, 1.0); 
  color.g = 1.0 - clamp(speed / estimatedMaxVelocity, 0.0, 1.0); 
  color.b = 0.5 * (1.0 - clamp(speed / estimatedMaxVelocity, 0.0, 1.0)); 
 

  out_color = vec4(color, 1.0); // Output the color with full opacity

}
