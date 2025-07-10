#include "camera.hpp"

#include <cmath>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
namespace rdm::gfx {
Camera::Camera() {
  eye = glm::vec3(2, 2, 2);
  target = glm::vec3(0, 2, 0);
  up = glm::vec3(0, 0, 1);
  fov = 90.f;
  nearPlane = 1.f;
  farPlane = 65535.f;
  p = Perspective;
  leftHanded = false;
  pdirty = true;
  vdirty = true;
}

Frustrum Camera::computeFrustrum() {
  glm::mat4 clipMatrix;
  const glm::mat4 v = vmatrix;
  const glm::mat4 p = pmatrix;

  clipMatrix[0][0] = v[0][0] * p[0][0] + v[0][1] * p[1][0] + v[0][2] * p[2][0] +
                     v[0][3] * p[3][0];
  clipMatrix[1][0] = v[0][0] * p[0][1] + v[0][1] * p[1][1] + v[0][2] * p[2][1] +
                     v[0][3] * p[3][1];
  clipMatrix[2][0] = v[0][0] * p[0][2] + v[0][1] * p[1][2] + v[0][2] * p[2][2] +
                     v[0][3] * p[3][2];
  clipMatrix[3][0] = v[0][0] * p[0][3] + v[0][1] * p[1][3] + v[0][2] * p[2][3] +
                     v[0][3] * p[3][3];
  clipMatrix[0][1] = v[1][0] * p[0][0] + v[1][1] * p[1][0] + v[1][2] * p[2][0] +
                     v[1][3] * p[3][0];
  clipMatrix[1][1] = v[1][0] * p[0][1] + v[1][1] * p[1][1] + v[1][2] * p[2][1] +
                     v[1][3] * p[3][1];
  clipMatrix[2][1] = v[1][0] * p[0][2] + v[1][1] * p[1][2] + v[1][2] * p[2][2] +
                     v[1][3] * p[3][2];
  clipMatrix[3][1] = v[1][0] * p[0][3] + v[1][1] * p[1][3] + v[1][2] * p[2][3] +
                     v[1][3] * p[3][3];
  clipMatrix[0][2] = v[2][0] * p[0][0] + v[2][1] * p[1][0] + v[2][2] * p[2][0] +
                     v[2][3] * p[3][0];
  clipMatrix[1][2] = v[2][0] * p[0][1] + v[2][1] * p[1][1] + v[2][2] * p[2][1] +
                     v[2][3] * p[3][1];
  clipMatrix[2][2] = v[2][0] * p[0][2] + v[2][1] * p[1][2] + v[2][2] * p[2][2] +
                     v[2][3] * p[3][2];
  clipMatrix[3][2] = v[2][0] * p[0][3] + v[2][1] * p[1][3] + v[2][2] * p[2][3] +
                     v[2][3] * p[3][3];
  clipMatrix[0][3] = v[3][0] * p[0][0] + v[3][1] * p[1][0] + v[3][2] * p[2][0] +
                     v[3][3] * p[3][0];
  clipMatrix[1][3] = v[3][0] * p[0][1] + v[3][1] * p[1][1] + v[3][2] * p[2][1] +
                     v[3][3] * p[3][1];
  clipMatrix[2][3] = v[3][0] * p[0][2] + v[3][1] * p[1][2] + v[3][2] * p[2][2] +
                     v[3][3] * p[3][2];
  clipMatrix[3][3] = v[3][0] * p[0][3] + v[3][1] * p[1][3] + v[3][2] * p[2][3] +
                     v[3][3] * p[3][3];

  Frustrum f;

  f.planes[Frustrum::Right].x = clipMatrix[3][0] - clipMatrix[0][0];
  f.planes[Frustrum::Right].y = clipMatrix[3][1] - clipMatrix[0][1];
  f.planes[Frustrum::Right].z = clipMatrix[3][2] - clipMatrix[0][2];
  f.planes[Frustrum::Right].w = clipMatrix[3][3] - clipMatrix[0][3];

  f.planes[Frustrum::Left].x = clipMatrix[3][0] + clipMatrix[0][0];
  f.planes[Frustrum::Left].y = clipMatrix[3][1] + clipMatrix[0][1];
  f.planes[Frustrum::Left].z = clipMatrix[3][2] + clipMatrix[0][2];
  f.planes[Frustrum::Left].w = clipMatrix[3][3] + clipMatrix[0][3];

  f.planes[Frustrum::Bottom].x = clipMatrix[3][0] + clipMatrix[1][0];
  f.planes[Frustrum::Bottom].y = clipMatrix[3][1] + clipMatrix[1][1];
  f.planes[Frustrum::Bottom].z = clipMatrix[3][2] + clipMatrix[1][2];
  f.planes[Frustrum::Bottom].w = clipMatrix[3][3] + clipMatrix[1][3];

  f.planes[Frustrum::Top].x = clipMatrix[3][0] - clipMatrix[1][0];
  f.planes[Frustrum::Top].y = clipMatrix[3][1] - clipMatrix[1][1];
  f.planes[Frustrum::Top].z = clipMatrix[3][2] - clipMatrix[1][2];
  f.planes[Frustrum::Top].w = clipMatrix[3][3] - clipMatrix[1][3];

  f.planes[Frustrum::Back].x = clipMatrix[3][0] - clipMatrix[2][0];
  f.planes[Frustrum::Back].y = clipMatrix[3][1] - clipMatrix[2][1];
  f.planes[Frustrum::Back].z = clipMatrix[3][2] - clipMatrix[2][2];
  f.planes[Frustrum::Back].w = clipMatrix[3][3] - clipMatrix[2][3];

  f.planes[Frustrum::Front].x = clipMatrix[3][0] + clipMatrix[2][0];
  f.planes[Frustrum::Front].y = clipMatrix[3][1] + clipMatrix[2][1];
  f.planes[Frustrum::Front].z = clipMatrix[3][2] + clipMatrix[2][2];
  f.planes[Frustrum::Front].w = clipMatrix[3][3] + clipMatrix[2][3];

  for (int i = 0; i < Frustrum::_Max; i++)
    f.planes[i] = glm::normalize(f.planes[i]);

  return f;
}

Frustrum::TestResult Frustrum::test(glm::vec3 min, glm::vec3 max) {
  TestResult r = Inside;

  for (int i = 0; i < Frustrum::_Max; i++) {
    float pos = planes[i].w;
    glm::vec3 norm = planes[i];
    glm::vec3 positive = min;
    glm::vec3 negative = max;

    if (norm.x >= 0.f) {
      positive.x = max.x;
      negative.x = min.x;
    }
    if (norm.y >= 0.f) {
      positive.y = max.y;
      negative.y = min.y;
    }
    if (norm.z >= 0.f) {
      positive.z = max.z;
      negative.z = min.z;
    }

    if (glm::dot(norm, positive) + pos < 0.f) return Outside;
    if (glm::dot(norm, negative) + pos < 0.f) r = Intersect;
  }
  return r;
}

void Camera::updateCamera(glm::vec2 framebufferSize) {
  if (pdirty || fbSize != framebufferSize) {
    switch (p) {
      case Perspective:
        pmatrix = glm::perspective(glm::radians(fov),
                                   framebufferSize.x / framebufferSize.y,
                                   nearPlane, farPlane);
        break;
      case Orthographic:
        pmatrix = glm::ortho(0.f, 1.f, 0.f, 1.f);
        break;
    }
    uipmatrix = glm::ortho(0.f, framebufferSize.x, 0.f, framebufferSize.y);
    fbSize = framebufferSize;
    pdirty = false;
  }
  if (vdirty) {
    if (leftHanded)
      vmatrix = glm::lookAtLH(eye, target, up);
    else
      vmatrix = glm::lookAt(eye, target, up);
    vdirty = false;
  }
}
}  // namespace rdm::gfx
