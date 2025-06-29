#version 330 core
layout(location = 0) out vec4 diffuseColor;

in vec2 f_uv;

uniform sampler2D texture0;

void main() {
  vec2 uv = vec2(f_uv.x, 1.0 - f_uv.y);
  diffuseColor = texture(texture0, uv);
}