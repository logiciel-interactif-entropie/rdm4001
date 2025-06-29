#version 330 core
layout (location = 0) in vec2 v_pos;

out vec2 f_uv;

uniform mat4 uiProjectionMatrix;
uniform vec2 scale = vec2(1);
uniform vec2 offset = vec2(0);

void main() {
  gl_Position = uiProjectionMatrix * vec4((v_pos * scale) + offset, 0.0, 1.0);
  f_uv = v_pos;
}