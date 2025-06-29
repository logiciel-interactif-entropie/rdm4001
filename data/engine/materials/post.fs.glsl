#version 330 core

in vec2 f_uv;
out vec4 o_color;

uniform sampler2DMS texture0;
uniform sampler2DMS texture1;

uniform float exposure = 1.0;
uniform float time;

uniform int banding_effect = 0xff3;
uniform vec2 target_res;
uniform float forced_aspect;
uniform vec2 window_res;
uniform bool bloom;

vec3 bandize(vec3 col) {
  vec3 out_color_raw = col;
  out_color_raw *= 255.0;
  ivec3 i_out_color_raw = ivec3(out_color_raw);
  i_out_color_raw.r = i_out_color_raw.r & banding_effect;
  i_out_color_raw.g = i_out_color_raw.g & banding_effect;
  i_out_color_raw.b = i_out_color_raw.b & banding_effect;
  out_color_raw = vec3(i_out_color_raw);
  out_color_raw /= 255.0;
  return out_color_raw;
}

void main() {
  const float gamma = 1.0;
  vec2 screen_uv = vec2(gl_FragCoord.x, gl_FragCoord.y);

  float blackbox = 0.0;
  if (forced_aspect != 0.0) {
    float aspectX = window_res.x * forced_aspect;
    blackbox = (window_res.x - aspectX) / 2;
    if (screen_uv.x < blackbox) {
      o_color = vec4(0.0);
      return;
    }
    if (screen_uv.x > aspectX + blackbox) {
      o_color = vec4(0.0);
      return;
    }
  }

  vec2 screen_uv2 = vec2(screen_uv.x - blackbox, screen_uv.y);
  vec2 _uv = screen_uv2 / vec2(window_res.x - blackbox, window_res.y);
  ivec2 uv = ivec2(_uv * target_res);

  /*vec4 base_color =
      (texelFetch(texture0, uv, 0) + texelFetch(texture0, uv, 1) +
       texelFetch(texture0, uv, 2) + texelFetch(texture0, uv, 3)) /
      4;*/
  vec3 base_color = texelFetch(texture0, uv, 0).rgb;

  if (bloom) {
    vec3 bloom_color = texelFetch(texture1, uv, 0).rgb;
    base_color += bloom_color;
  }
  vec3 result = base_color.rgb;
  // vec3 result = vec3(1.0) - exp(-base_color * exposure);
  // result = pow(result, vec3(1.0 / gamma));
  o_color = vec4(result, 1.0);
}
