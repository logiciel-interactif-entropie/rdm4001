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

uniform float shininess = 0.0;
uniform vec3 view_position;
uniform vec3 sun_direction = vec3(-0.5, 0.5, 0.5);

void main() {
  vec3 i = normalize(v_fmpos - view_position);
  vec3 r = reflect(i, normalize(v_fnormal));

  float intensity = dot(v_fnormal, normalize(sun_direction));
  vec3 result = vec3(0.5, 0.5, 0.5) + vec3(0.5, 0.5, 0.5) * intensity;
  float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
  f_color = vec4(result, 1.0);
  f_bloom = vec4(f_color.rgb * brightness, 1.0);
}
