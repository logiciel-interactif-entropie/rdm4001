#version 330 core
layout(location = 0) in vec3 v_position;

uniform vec3 from;
uniform vec3 to;
uniform vec3 color;

out vec4 v_fposition;
out vec3 v_fcolor;

uniform mat4 model = mat4(1);
uniform mat4 viewMatrix = mat4(1);
uniform mat4 projectionMatrix = mat4(1);

void main() {
  mat4 pv = projectionMatrix * viewMatrix;
  vec3 p = (v_position * to);
  if (length(p) == 0.0) p = from;
  vec4 pos = pv * model * vec4(p, 1.0);
  gl_Position = pos;
  v_fcolor = color;
}
