#version 330 core
const int MAX_BONES = 128;
const int MAX_BONE_INFLUENCE = 4;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;
layout(location = 3) in ivec4 v_bone_ids;
layout(location = 4) in vec4 v_weights;

layout(std140) uniform BoneTransformBlock { mat4 boneTransform[MAX_BONES]; };

uniform mat4 model = mat4(1);
uniform mat4 viewMatrix = mat4(1);
uniform mat4 projectionMatrix = mat4(1);

out vec4 v_fcolor;
out vec3 v_fnormal;
out vec3 v_fmpos;
out vec4 v_fvpos;
out vec3 v_fvnorm;
out vec4 v_fpos;
out vec2 v_fuv;
out vec3 v_fraydir;
out vec4 v_ftotal_pos;

void main() {
  vec4 total_pos = vec4(0.0);
  int null_ids = 0;
  for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
    if (v_bone_ids[i] == -1) {
      null_ids++;
      continue;
    }
    if (v_bone_ids[i] >= MAX_BONES) {
      total_pos = vec4(v_position, 1.0f);
      break;
    }
    vec4 local_position = boneTransform[v_bone_ids[i]] * vec4(v_position, 1.0);
    total_pos += local_position * v_weights[i];
  }
  if (null_ids == 4) total_pos = vec4(v_position, 1.0);

  v_ftotal_pos = total_pos;
  // total_pos = vec4(v_position, 1.0f);

  v_fvnorm = mat3(viewMatrix * model) * v_normal;
  v_fmpos = vec3(model * total_pos);
  mat4 pv = projectionMatrix * viewMatrix;
  vec4 pos = pv * total_pos;
  v_fvpos = viewMatrix * total_pos;
  v_fpos = pos;
  gl_Position = pv * model * total_pos;
  v_fcolor = vec4(0.5, 0.5, 0.5, 1.0);
  v_fnormal = v_normal;
  v_fuv = v_uv;
}
