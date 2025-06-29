#version 330 core

in vec2 f_uv;
layout(location = 0) out vec4 o_color;
layout(location = 1) out vec4 o_bloom;

uniform float time;
uniform vec3 camera_position;
uniform vec3 camera_target;
uniform sampler2D texture0;

#define EPS 0.01

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

float distance_from_sphere(in vec3 p, in vec3 c, float r) {
  return length(p - c) - r;
}
float distance_from_box(vec3 p, vec3 b) {
  vec3 d = abs(p) - b;
  return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

float distance_from_plane(in vec3 pos, in vec4 normal) {
  float distCenter = distance(vec2(0, 0), pos.xz);
  float map =
      noise(vec3(pos.xz / 100, 0.0)) * (1.0 + (min(100, distCenter) * 0.6));
  return (dot(pos, normal.xyz) + (normal.w));
}

float intersect(float distA, float distB) { return max(distA, distB); }

float unioncsg(float distA, float distB) { return min(distA, distB); }

float difference(float distA, float distB) { return max(distA, -distB); }

struct Intersect {
  int idx;
  float dist;
};

#define SPHERES 0
#define PLANE 1
#define FLAT 2
#define WARP 3

float sierpinskiPyramid(vec3 pt) {
  vec3 ori = vec3(0.0, 2.5, 0.0);
  vec3 a1 = vec3(1, 1, 1) + ori;
  vec3 a2 = vec3(-1, -1, 1) + ori;
  vec3 a3 = vec3(1, -1, -1) + ori;
  vec3 a4 = vec3(-1, 1, -1) + ori;

  vec3 c;
  int n = 0;
  float dist, d;
  float scale = 2.;
  while (n < 16) {
    c = a1;
    dist = length(pt - a1);
    d = length(pt - a2);
    if (d < dist) {
      c = a2;
      dist = d;
    }
    d = length(pt - a3);
    if (d < dist) {
      c = a3;
      dist = d;
    }
    d = length(pt - a4);
    if (d < dist) {
      c = a4;
      dist = d;
    }
    pt = scale * pt - c * (scale - 1.0);
    n++;
  }

  return length(pt) * pow(scale, float(-n));
}

struct Entity {
  int type;
  vec3 position;
  float user0;
};

#define MAX_ENTITIES 100
uniform Entity entities[MAX_ENTITIES];
uniform int numEntities;

Intersect map_the_world(in vec3 p) {
  /*vec3 s = vec3(10, 10, 10);
  vec3 q = p - s*round(p/s);

  float displacement = sin(5.0 * p.x) * sin(5.0 * p.y) * sin(5.0 * p.z) * 0.25;
  float sphere_0 = distance_from_sphere(p, vec3(-2.0, 1.0, -2.0), 1.0);
  float sphere_1 = distance_from_sphere(p, vec3(-4.0, 1.0, -2.0), 1.0);
  float plane_0 = distance_from_plane(p, vec4(0.0, 1.0, 0.0, 2.2));
  float plane_1 = distance_from_plane(p, vec4(1.0, 0.0, 0.0, 2.2));
  float box_0 = distance_from_box(p, vec3(1,19,1));
  float box_1 = distance_from_box(p, vec3(3,13,0.75));
  float sierpinski = sierpinskiPyramid(q);

  Intersect res;
  res.idx = SPHERES;
  //res.dist = unioncsg(plane_0, unioncsg(sphere_4, unioncsg(unioncsg(sphere_0,
  sphere_1), unioncsg(sphere_2, sphere_3)))); res.dist = unioncsg(plane_0,
  unioncsg(unioncsg(box_0, box_1), sphere_0)); if(res.dist == sphere_0) res.idx
  = SPHERES; else if(res.dist == plane_0 ) res.idx = PLANE; else if(res.dist ==
  sierpinski || res.dist == sphere_1) res.idx = WARP; else if(res.dist == box_0)
    res.idx = SPHERES;*/

  Intersect res;
  float dist = 1000.0;
  for (int i = 0; i < numEntities; i++) {
    Entity ent = entities[i];
    float dfun = 1000.0;
    switch (ent.type) {
      case FLAT:
      case SPHERES:
        dfun = distance_from_sphere(p, ent.position, ent.user0);
        break;
      case PLANE:
        dfun = distance_from_box(p, vec3(50, 1, 50));
        break;
    }
    dist = unioncsg(dist, dfun);
    if (dist == dfun) res.idx = ent.type;
  }
  res.dist = dist;
  return res;
}

vec3 calculate_normal(vec3 p) {
  return normalize(vec3(map_the_world(vec3(p.x + EPS, p.y, p.z)).dist -
                            map_the_world(vec3(p.x - EPS, p.y, p.z)).dist,
                        map_the_world(vec3(p.x, p.y + EPS, p.z)).dist -
                            map_the_world(vec3(p.x, p.y - EPS, p.z)).dist,
                        map_the_world(vec3(p.x, p.y, p.z + EPS)).dist -
                            map_the_world(vec3(p.x, p.y, p.z - EPS)).dist));
}

mat3 viewMatrix(vec3 eye, vec3 center, vec3 up) {
  vec3 f = normalize(center - eye);
  vec3 s = normalize(cross(f, up));
  vec3 u = cross(s, f);
  return mat3(vec3(s), vec3(u), vec3(-f));
}

struct Point {
  vec3 pos;
  vec3 col;
  vec3 norm;
  int idx;
  float maxClosest;
  float dist;
};

#define BOARD_SQUARE 1.0

vec3 boardColor(vec3 p) {
  vec3 pos = vec3(p.x, p.y, p.z);

  if ((mod(pos.x, (BOARD_SQUARE * 2.0)) < BOARD_SQUARE &&
       mod(pos.z, (BOARD_SQUARE * 2.0)) > BOARD_SQUARE) ||
      (mod(pos.x, (BOARD_SQUARE * 2.0)) > BOARD_SQUARE &&
       mod(pos.z, (BOARD_SQUARE * 2.0)) < BOARD_SQUARE))
    return vec3(1.0, 1.0, 0.0);
  else
    return vec3(0.0, 1.0, 0.0);
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

float getShadow(in vec3 ro, in vec3 rd, in float min_t, in float max_t,
                in float k) {
  float res = 1.0;
  float cloud = getCloud(ro, rd);
  for (float t = min_t; t < max_t;) {
    float dist = map_the_world(ro + rd * t).dist;
    if (dist < EPS) return 0.0;
    res = min(res, k * dist / t);
    t += dist;
  }
  return min(res, cloud);
}

vec3 sun_pos = vec3(5.0, 5.0, -5.0);
Point ray_march(in vec3 ro, in vec3 rd) {
  float total_distance_traveled = 0.0;
  const int NUMBER_OF_STEPS = 200;
  int num_steps = 0;
  const float MINIMUM_HIT_DISTANCE = EPS;
  const float MAXIMUM_TRACE_DISTANCE = 1000.0;

  float maxClosest = 1.0;
  Point p;
  p.pos = vec3(0.0);
  for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
    num_steps += 1;
    vec3 current_position = (ro + (total_distance_traveled * rd));

    Intersect intersect = map_the_world(current_position);
    maxClosest = min(intersect.dist, maxClosest);
    p.pos = current_position;
    p.dist = total_distance_traveled;
    p.maxClosest = maxClosest;

    vec3 light_position = sun_pos * 100000;
    if (intersect.dist < MINIMUM_HIT_DISTANCE) {
      if (intersect.idx == WARP) {
        intersect.dist = 0.1;
      } else {
        vec3 normal = calculate_normal(current_position);
        vec3 norm = normalize(normal);
        vec3 direction_to_light = normalize(light_position);
        vec3 direction_to_light2 = normalize(light_position - current_position);
        vec3 reflect_dir = reflect(direction_to_light2, norm);
        float diffuse_intensity = max(0.0, dot(normal, direction_to_light));
        float specular_intensity = pow(max(dot(rd, reflect_dir), 0.0), 32);
        // p.col = normal/3;
        p.norm = normal;
        p.idx = intersect.idx;
        vec3 base_color = vec3(0.0);
        if (intersect.idx == PLANE) base_color = boardColor(p.pos) * 0.6;
        if (intersect.idx == FLAT) base_color = vec3(0.5);

        float shadow = getShadow(p.pos + (direction_to_light2),
                                 direction_to_light2, 0.0, 100.0, 8.0);

        vec3 blinn_phong =
            base_color * diffuse_intensity + base_color * specular_intensity;
        p.col = base_color;

        p.col += blinn_phong * shadow;
        // p.col = current_position / 2;
        // return mix(vec3(0.02,0.61,0.86),vec3(0.2,0.34,1.0),maxClosest);
        // p.col = base_color;
        float updot = dot(rd, vec3(0.0, 1.0, 0.0));
        vec3 fogcolor =
            mix(vec3(0.77, 0.79, 1.0), vec3(0.16, 0.20, 0.96), updot + 0.5);
        p.col = mix(
            p.col, fogcolor,
            min(total_distance_traveled / (MAXIMUM_TRACE_DISTANCE / 2), 1.0));
        return p;
      }
    }

    if (total_distance_traveled > MAXIMUM_TRACE_DISTANCE) {
      float updot = dot(rd, vec3(0.0, 1.0, 0.0));
      p.col = mix(vec3(0.77, 0.79, 1.0), vec3(0.16, 0.20, 0.96), updot + 0.5);
      float cloudNoise = getCloud(camera_position, rd);
      p.col = mix(p.col, vec3(1, 1, 1), max(0.0, cloudNoise));
      float sundot = dot(rd, normalize(light_position));
      if (sundot > 0.995) p.col = vec3(1.0, 1.0, 1.0);
      return p;
    }
    total_distance_traveled += intersect.dist;
  }

  float updot = dot(rd, vec3(0.0, 1.0, 0.0));
  p.col = mix(vec3(0.77, 0.79, 1.0), vec3(0.16, 0.20, 0.96), updot);
  float sundot = dot(rd, normalize(sun_pos));
  if (sundot > 0.995) p.col = vec3(1.0, 1.0, 1.0);
  return p;
}

vec4 trace(vec3 ro, vec3 rd) {
  Point a = ray_march(ro, rd);
  vec3 color = a.col;
  if (a.norm != vec3(0.0) && a.idx == SPHERES) {
    vec3 rdf = rd;
    Point p;
    p.norm = a.norm;
    p.pos = a.pos;
    float fac = 0.6;
    for (int i = 0; i < 4; i++) {
      vec3 norm = p.norm;
      rdf = rdf - (2.0 * dot(rdf, norm) * norm);
      p = ray_march(p.pos + (rdf * 0.01), normalize(rdf));
      color = mix(color, p.col, fac);
      fac -= 0.1;
      if (p.idx != SPHERES || p.norm == vec3(0.0)) break;
    }
  }
  return vec4(color, a.dist);
}

void main() {
  vec2 uv = f_uv * 2.0 - 1.0;
  vec3 ro = camera_position;
  mat3 vm;
  vec3 rd;

  vec4 color;
  vm = viewMatrix(camera_target + vec3(0.0, 0.0, 0.0), ro, vec3(0.0, 1.0, 0.0));
  rd = normalize(vm * vec3(uv, 1.0));
  color = trace(ro, rd);

  // vec4 base_color = texture(texture0, f_uv);
  o_color = vec4(color.xyz, 1.0);
  float brightness =
      max(dot(o_color.rgb, vec3(0.2126, 0.7152, 0.0722)) - 0.5, 0);
  o_bloom = vec4(o_color.rgb * brightness, 1.0);
  gl_FragDepth = 1.0 - (1.0 / color.w);
}
