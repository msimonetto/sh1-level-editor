#pragma once
#include "raylib.h"

struct Frustum {
  struct Plane {
    float a, b, c, d;
    float Distance(const Vector3 &p) const;
  };

  Plane planes[6]; // 0: Left, 1: Right, 2: Bottom, 3: Top, 4: Near, 5: Far

  static Frustum FromCamera(const Camera3D &camera, float aspectRatio);
  bool IsPointInFrustum(const Vector3 &p) const;
  bool IsBoxInFrustum(const BoundingBox &box) const;
};
