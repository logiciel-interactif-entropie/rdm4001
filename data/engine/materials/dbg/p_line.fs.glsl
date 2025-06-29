#version 330 core

in vec4 v_fposition;
in vec3 v_fcolor;

layout(location = 0) out vec4 f_color;

void main() { f_color = vec4(v_fcolor, 1.0); }
