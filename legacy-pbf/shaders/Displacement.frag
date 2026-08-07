#version 330

uniform vec3 u_cam_pos;
uniform vec3 u_light_pos;
uniform vec3 u_light_intensity;

uniform vec4 u_color;

uniform sampler2D u_texture_2;
uniform vec2 u_texture_2_size;

uniform float u_normal_scaling;
uniform float u_height_scaling;

in vec4 v_position;
in vec4 v_normal;
in vec4 v_tangent;
in vec2 v_uv;

out vec4 out_color;

float h(vec2 uv) {
  return texture(u_texture_2, uv).x;
}

void main() {

  vec3 b = cross(v_normal.xyz, v_tangent.xyz);

  mat3 tbn = mat3(v_tangent.xyz, b, v_normal.xyz);

  float dU = (h(vec2(v_uv.x + 1.0/u_texture_2_size.x, v_uv.y)) - h(vec2(v_uv.x, v_uv.y))) * u_height_scaling * u_normal_scaling;
  float dV = (h(vec2(v_uv.x, v_uv.y + 1.0/u_texture_2_size.y)) - h(vec2(v_uv.x, v_uv.y))) * u_height_scaling * u_normal_scaling;

  vec3 n_0 = vec3(-dU, -dV, 1.0);

  vec3 n_d = normalize(tbn * n_0);

  vec3 ambient = 0.1*vec3(1.0, 1.0, 1.0);

  vec3 l = u_light_pos - vec3(v_position);
  float r_square = dot(l,l);
  vec3 diffuse = vec3(u_color) * (u_light_intensity / r_square) * max(0, dot(normalize(n_d), normalize(l)));

  float k_s = 0.5;
  vec3 v = normalize(u_cam_pos - v_position.xyz);
  vec3 h = (v + normalize(l)) / length(v + normalize(l));
  vec3 spec = k_s * u_light_intensity / dot(l, l) * pow(max(0, dot(normalize(n_d), normalize(h))), 100);

  out_color = vec4(ambient+diffuse+spec, 1.0);
}

