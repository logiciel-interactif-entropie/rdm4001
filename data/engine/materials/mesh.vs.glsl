#version 330 core
layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;

uniform mat4 model = mat4(1);
uniform mat4 viewMatrix = mat4(1);
uniform mat4 projectionMatrix = mat4(1);

out vec4 v_fcolor;
out vec3 v_fnormal;
out vec3 v_fmpos;
out vec4 v_fvpos;
out vec3 v_fvnorm;
out vec4 v_fpos;
out vec2 v_fuv;
out vec3 v_fraydir;

void main() {
  v_fvnorm = mat3(viewMatrix * model) * v_normal;
  v_fmpos = vec3(model * vec4(v_position, 1.0));
  mat4 pv = projectionMatrix * viewMatrix;
  vec4 pos = pv * model * vec4(v_position, 1.0);
  v_fvpos = viewMatrix * vec4(v_position, 1.0);
  v_fpos = pos;
  gl_Position = pos;
  v_fcolor = vec4(0.5, 0.5, 0.5, 1.0);
  v_fnormal = v_normal;
  v_fuv = v_uv;
}
