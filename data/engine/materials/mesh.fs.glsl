#version 330 core
#define PI 3.1415926

#ifdef DEFERRED_RENDERING
layout(location = 0) out vec4 f_fragpos;
layout(location = 1) out vec4 f_normal;
layout(location = 2) out vec4 f_albedo;
layout(location = 3) out vec4 f_material_properties;
#else
layout(location = 0) out vec4 f_color;
layout(location = 1) out vec4 f_bloom;
#endif

in vec4 v_fcolor;  // the input variable from the vertex shader (same name and
                   // same type)
in vec3 v_fvnorm;
in vec3 v_fnormal;
in vec3 v_fmpos;
in vec2 v_fuv;
in vec4 v_fvpos;
in vec3 v_fmeshpos;
in vec2 v_flm_uv;

struct light {
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  vec3 position;
};

layout(std140) uniform SunBlock {
  vec3 sun_ambient;
  vec3 sun_diffuse;
  vec3 sun_specular;
  vec3 sun_direction;
};

// pbr_properties.x = roughness
// pbr_properties.y = metallic
// pbr_properties.z = specular
layout(std140) uniform Material {
  vec4 albedo;
  vec4 pbr_properties;
};

layout(std140) uniform Light {
  vec4 position;
  vec4 diffuse;
  vec4 specular;
}
lights[8];

uniform sampler2D albedo_texture;

uniform float shininess = 0.0;
uniform vec3 camera_position;
uniform mat4 model = mat4(1);

void main() {
  vec3 base = (albedo * texture(albedo_texture, v_fuv)).xyz;
#ifdef DEFERRED_RENDERING
  f_albedo = vec4(base, 1.0);
  f_normal = vec4(v_fnormal, 1.0);
  f_fragpos = vec4(v_fmpos, 1.0);
#else
  vec3 P = v_fmeshpos;
  vec3 L = normalize(sun_direction * mat3(model));
  vec3 N = normalize(v_fnormal);
  vec3 V = normalize(vec3(1));
  vec3 H = normalize(L + V);

  float NdL = max(0.0, dot(N, L));
  float NdV = max(0.001, dot(N, V));
  float NdH = max(0.001, dot(N, H));
  float HdV = max(0.001, dot(H, V));
  float LdV = max(0.001, dot(L, V));

  float roughness = pbr_properties.x;
  float metallic = pbr_properties.y;

  vec3 specular = mix(vec3(0.04), base, metallic);

  // phong model
  vec3 R = reflect(-L, N);
  float spec = max(0.0, dot(V, R));
  float k = 1.999 / (roughness * roughness);

  vec3 frenselSpecular = mix(specular, vec3(1.0), pow(1.01 - NdV, 5.0));
  vec3 valueSpecular =
      min(1.0, 3.0 * 0.0398 * k) * pow(spec, min(10000.0, k)) * specular;
  valueSpecular *= vec3(NdL);

  vec3 valueDiffuse = (vec3(1.0) - frenselSpecular) * (1.0 / PI) * NdL;

  vec3 light_in = vec3(1.0) * sun_ambient;
  vec3 light_reflected = valueSpecular * sun_specular;
  vec3 light_diffuse = valueDiffuse * sun_diffuse;

  vec3 result = sun_ambient + (light_diffuse * mix(base, vec3(0.0), metallic)) +
                light_reflected;
  float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
  f_color = vec4(result, 1.0);
  f_bloom = vec4(f_color.rgb * brightness, 1.0);
#endif
}
