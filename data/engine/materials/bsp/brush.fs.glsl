#version 330 core
layout(location = 0) out vec4 f_color;
layout(location = 1) out vec4 f_bloom;

in vec4 v_fcolor;  // the input variable from the vertex shader (same name and
                   // same type)
in vec3 v_fnormal;
in vec3 v_fmpos;
in vec2 v_fuv;
in vec2 v_flm_uv;

struct light {
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  vec3 position;
};

uniform sampler2D texture0;
uniform sampler2D texture1;
// uniform samplerCube skybox;
uniform float shininess = 0.0;
uniform float gamma = 4.0;
uniform vec3 camera_position;

vec4 cubic(float v) {
  vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
  vec4 s = n * n * n;
  float x = s.x;
  float y = s.y - 4.0 * s.x;
  float z = s.z - 4.0 * s.y + 6.0 * s.x;
  float w = 6.0 - x - y - z;
  return vec4(x, y, z, w) * (1.0 / 6.0);
}

vec4 textureBicubic(sampler2D sampler, vec2 texCoords) {
  vec2 texSize = textureSize(sampler, 0);
  vec2 invTexSize = 1.0 / texSize;

  texCoords = texCoords * texSize - 0.5;

  vec2 fxy = fract(texCoords);
  texCoords -= fxy;

  vec4 xcubic = cubic(fxy.x);
  vec4 ycubic = cubic(fxy.y);

  vec4 c = texCoords.xxyy + vec2(-0.5, +1.5).xyxy;

  vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
  vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;

  offset *= invTexSize.xxyy;

  vec4 sample0 = texture(sampler, offset.xz);
  vec4 sample1 = texture(sampler, offset.yz);
  vec4 sample2 = texture(sampler, offset.xw);
  vec4 sample3 = texture(sampler, offset.yw);

  float sx = s.x / (s.x + s.y);
  float sy = s.z / (s.z + s.w);

  return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}

void main() {
  vec3 i = normalize(v_fmpos - camera_position);
  vec3 r = reflect(i, normalize(v_fnormal));

  vec4 samplet = texture2D(texture0, vec2(v_fuv.x, -v_fuv.y));
  // vec4 samplel = textureBicubic(texture1, v_flm_uv) * gamma;
  vec4 samplel = texture2D(texture1, v_flm_uv) * gamma;
  //  vec4 samples = texture(skybox, vec3(r.x, -r.z, r.y)) * shininess;

  // float intensity = dot(v_fnormal, normalize(vec3(0.5, 0.5, 0.5)));
  vec3 result = samplet.xyz * samplel.xyz /* + samples.xyz*/;
  // vec3 result = vec3(0.2) + (intensity * vec3(0.8));
  //  float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
  float brightness = max(dot(result, vec3(0.2126, 0.7152, 0.0722)) - 0.5, 0);
  f_color = vec4(result, 1.0);
  f_bloom = vec4(f_color.rgb * brightness, 1.0);
}
