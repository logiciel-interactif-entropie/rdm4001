#version 330 core
layout(location = 0) out vec4 diffuseColor;

in vec2 f_uv;

uniform sampler2D texture0;
uniform vec3 color;
uniform vec4 bgcolor = vec4(0);

void main() {
  vec4 o_color = texture(texture0, f_uv) * vec4(color, 1.0);
  if (length(bgcolor) == 0.0)
    diffuseColor = o_color;
  else
    diffuseColor = ((1.0 - o_color.a) * bgcolor) + (o_color.a * o_color);
}
