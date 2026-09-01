#include "viewport/Frustum.h"
#include "raymath.h"
#include <cmath>

float Frustum::Plane::Distance(const Vector3 &p) const {
  return a * p.x + b * p.y + c * p.z + d;
}

Frustum Frustum::FromCamera(const Camera3D &camera, float aspectRatio) {
  Frustum f;
  Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);
  Matrix matProj;

  if (camera.projection == CAMERA_PERSPECTIVE) {
    matProj = MatrixPerspective(camera.fovy * DEG2RAD, aspectRatio, 0.01f, 2000.0f);
  } else {
    float orthoScale = camera.fovy * 0.5f;
    matProj = MatrixOrtho(-orthoScale * aspectRatio, orthoScale * aspectRatio,
                          -orthoScale, orthoScale, 0.01f, 2000.0f);
  }

  Matrix m = MatrixMultiply(matView, matProj);

  // Left plane (X_clip + W_clip >= 0)
  f.planes[0] = {m.m0 + m.m3, m.m4 + m.m7, m.m8 + m.m11, m.m12 + m.m15};
  // Right plane (W_clip - X_clip >= 0)
  f.planes[1] = {m.m3 - m.m0, m.m7 - m.m4, m.m11 - m.m8, m.m15 - m.m12};
  // Bottom plane (Y_clip + W_clip >= 0)
  f.planes[2] = {m.m1 + m.m3, m.m5 + m.m7, m.m9 + m.m11, m.m13 + m.m15};
  // Top plane (W_clip - Y_clip >= 0)
  f.planes[3] = {m.m3 - m.m1, m.m7 - m.m5, m.m11 - m.m9, m.m15 - m.m13};
  // Near plane (Z_clip + W_clip >= 0)
  f.planes[4] = {m.m2 + m.m3, m.m6 + m.m7, m.m10 + m.m11, m.m14 + m.m15};
  // Far plane (W_clip - Z_clip >= 0)
  f.planes[5] = {m.m3 - m.m2, m.m7 - m.m6, m.m11 - m.m10, m.m15 - m.m14};

  // Normalize plane equations
  for (int i = 0; i < 6; i++) {
    float len = sqrtf(f.planes[i].a * f.planes[i].a +
                      f.planes[i].b * f.planes[i].b +
                      f.planes[i].c * f.planes[i].c);
    if (len > 0.00001f) {
      f.planes[i].a /= len;
      f.planes[i].b /= len;
      f.planes[i].c /= len;
      f.planes[i].d /= len;
    }
  }
  return f;
}

bool Frustum::IsPointInFrustum(const Vector3 &p) const {
  for (int i = 0; i < 6; i++) {
    if (planes[i].Distance(p) < -0.1f)
      return false;
  }
  return true;
}

bool Frustum::IsBoxInFrustum(const BoundingBox &box) const {
  // If box bounds are invalid/uninitialized, treat as visible
  if (box.min.x > box.max.x)
    return true;

  for (int i = 0; i < 6; i++) {
    Vector3 positive = {
        (planes[i].a > 0.0f) ? box.max.x : box.min.x,
        (planes[i].b > 0.0f) ? box.max.y : box.min.y,
        (planes[i].c > 0.0f) ? box.max.z : box.min.z};
    if (planes[i].Distance(positive) < 0.0f)
      return false;
  }
  return true;
}
