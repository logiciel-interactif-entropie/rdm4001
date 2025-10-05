#version 330 core

layout(location = 0) out vec4 f_color;
layout(location = 1) out vec4 f_bloom;

uniform sampler2D gbuffer_position;
uniform sampler2D gbuffer_normal;
uniform sampler2D gbuffer_albedo;
uniform sampler2D gbuffer_material_properties;

uniform vec3 camera_position;

in vec2 f_uv;

struct light {
  vec4 position;
  vec4 diffuse;
  vec4 specular;
};

#define NR_LIGHTS 16
layout(std140) uniform Light { light lights[NR_LIGHTS]; };
uniform int lightCount;

layout(std140) uniform SunBlock {
  vec4 sun_ambient;
  vec4 sun_diffuse;
  vec4 sun_specular;
  vec4 sun_direction;
};

void main() {
  vec3 fragPos = texture(gbuffer_position, f_uv).xyz;
  vec3 normal = texture(gbuffer_normal, f_uv).xyz;
  vec3 albedo = texture(gbuffer_albedo, f_uv).xyz;
  vec4 material_properties = texture(gbuffer_material_properties, f_uv);

  float specular = material_properties.z;
  vec3 lighting = albedo + sun_ambient.xyz;
  vec3 viewDir = normalize(camera_position - fragPos);
  for (int i = 0; i < lightCount; i++) {
    vec3 lightDir = normalize(lights[i].position.xyz - fragPos);
    vec3 diffuse =
        max(dot(normal, lightDir), 0.0) * albedo * lights[i].diffuse.xyz;
    lighting += diffuse;
  }
  vec3 lightDir = normalize(sun_direction.xyz - fragPos);
  vec3 diffuse = max(dot(normal, lightDir), 0.0) * albedo * sun_diffuse.xyz;
  lighting += diffuse;

  f_color = vec4(lighting, 1.0);
}
