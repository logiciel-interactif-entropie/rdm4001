#version 330 core
out vec4 FragColor;

// https://learnopengl.com/Advanced-Lighting/Bloom

in vec2 f_uv;

uniform sampler2DMS image;

uniform bool horizontal;
uniform float weight[5] =
    float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
  ivec2 uv = ivec2(gl_FragCoord.x, gl_FragCoord.y);
  // vec2 tex_offset = 1.0 / textureSize(image, 0);  // gets size of single
  // texel
  vec3 result = texelFetch(image, uv, 0).rgb *
                weight[0];  // current fragment's contribution
  int fac = 2;
  if (horizontal) {
    for (int i = 1; i < 5; ++i) {
      result += texelFetch(image, uv + ivec2(i * fac, 0.0), 0).rgb * weight[i];
      result += texelFetch(image, uv - ivec2(i * fac, 0.0), 0).rgb * weight[i];
    }
  } else {
    for (int i = 1; i < 5; ++i) {
      result += texelFetch(image, uv + ivec2(0.0, i * fac), 0).rgb * weight[i];
      result += texelFetch(image, uv - ivec2(0.0, i * fac), 0).rgb * weight[i];
    }
  }
  FragColor = vec4(result, 1.0);
}
