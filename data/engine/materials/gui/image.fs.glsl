#version 330 core
layout(location = 0) out vec4 diffuseColor;

in vec2 f_uv;

uniform sampler2D texture0;
uniform vec3 color;
uniform vec4 bgcolor;

void main() {
  vec4 o_color = texture(texture0, f_uv) * vec4(color, 1.0);
  if (length(bgcolor) == 0.0)
    diffuseColor = o_color;
  else
    diffuseColor = mix(bgcolor, vec4(o_color), o_color.a);
}
