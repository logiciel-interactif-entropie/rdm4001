#version 330 core
layout(location = 0) out vec4 f_color;
layout(location = 1) out vec4 f_bloom;

in vec4 v_fcolor;  // the input variable from the vertex shader (same name and
                   // same type)
in vec3 v_fnormal;
in vec3 v_fmpos;
in vec4 v_fvpos;
in vec2 v_fuv;
in vec2 v_flm_uv;

uniform vec3 camera_position;
uniform float time;

float mod289(float x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 perm(vec4 x) { return mod289(((x * 34.0) + 1.0) * x); }

float noise(vec3 p) {
  vec3 a = floor(p);
  vec3 d = p - a;
  d = d * d * (3.0 - 2.0 * d);

  vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
  vec4 k1 = perm(b.xyxy);
  vec4 k2 = perm(k1.xyxy + b.zzww);

  vec4 c = k2 + a.zzzz;
  vec4 k3 = perm(c);
  vec4 k4 = perm(c + 1.0);

  vec4 o1 = fract(k3 * (1.0 / 41.0));
  vec4 o2 = fract(k4 * (1.0 / 41.0));

  vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
  vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

  return o4.y * d.y + o4.x * (1.0 - d.y);
}

float getCloud(in vec3 ro, in vec3 rd) {
  float updot = dot(rd, vec3(0.0, 1.0, 0.0));
  float timed = 4;
  vec3 cloudPos =
      (ro / 100) +
      vec3(rd.x + (time / timed), rd.y + (time / timed), rd.z + (time / timed));
  float cloudNoise = noise(cloudPos * 2) * max(0.0, updot);
  float cloudNoise2 =
      noise(cloudPos * 4 + vec3(time, 0, 0)) * max(0.0, updot - 0.7);
  return cloudNoise + cloudNoise2;
}

uniform mat4 viewMatrix;

uniform samplerCube skybox;

void main() {
  vec3 i = normalize(v_fvpos.xyz);
  vec3 viewR = reflect(i, normalize(v_fnormal));
  vec3 worldR = inverse(mat3(viewMatrix)) * i;
  f_color = texture(skybox, vec3(worldR.x, -worldR.z, worldR.y));
  /*float brightness = dot(f_color.xyz, vec3(0.2126, 0.7152, 0.0722));
  if (brightness > 0.2)
    f_bloom = f_color;
  else
    f_bloom = vec4(0.0, 0.0, 0.0, 1.0);*/
  float brightness =
      max(dot(f_color.rgb, vec3(0.2126, 0.7152, 0.0722)) - 0.5, 0);
  f_bloom = vec4(f_color.rgb * brightness, 1.0);
}
