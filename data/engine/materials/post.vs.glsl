#version 330 core

out vec2 f_uv;

void main() {  
  vec2 positions[3];
  positions[0] = vec2(-1.0,-1.0);
  positions[1] = vec2(-1.0,3.0);
  positions[2] = vec2(3.0,-1.0);

  vec2 uvs[3];
  uvs[0] = vec2(-1.0,-1.0);
  uvs[1] = vec2(-1.0,3.0);
  uvs[2] = vec2(3.0,-1.0);

  f_uv = (uvs[gl_VertexID] * vec2(0.5)) + vec2(0.5);

  gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}