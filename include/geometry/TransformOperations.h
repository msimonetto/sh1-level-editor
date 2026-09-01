#pragma once
#include "raylib.h"
#include <cstdint>

namespace Geometry {

constexpr float Pi               = 3.1415927f;
constexpr float DegreesToRadians = Pi / 180.0f;
constexpr float RadiansToDegrees = 180.0f / Pi;

// 3D Vector & Normal utilities
Vector3 ComputeTriangleNormal(Vector3 v0, Vector3 v1, Vector3 v2, Vector3 fallback = { 0.0f, 1.0f, 0.0f });

// UV & Polygon utilities
uint8_t NormalizedToByteUv(float uv);
float   ByteToNormalizedUv(uint8_t raw);
void    ComputeUvBounds(const float uv[4][2], int numVerts, float& minU, float& maxU, float& minV, float& maxV);
void    InvertPolygonWinding(uint8_t v[4], float uv[4][2], uint8_t rawU[4], uint8_t rawV[4]);
void    RotatePolygonUv(float uv[4][2], uint8_t rawU[4], uint8_t rawV[4], int numVerts, int steps);

// Matrix algebra utilities
void Matrix3x3Multiply(const float a[3][3], const float b[3][3], float out[3][3]);
void CreateAxisAngleMatrix(Vector3 axis, float angleRad, float outMatrix[3][3]);
Vector3 TransformPointAroundPivot(Vector3 point, Vector3 pivot, const float rotMatrix[3][3]);

// Rotation
//// Matrix rescaling/reflection: PS1 <-> Viewport
void RescaleEngineToViewport(const int16_t inMatrix[3][3], float outMatrix[3][3]);
void RescaleViewportToEngine(const float inMatrix[3][3], int16_t outMatrix[3][3]);

//// Operations
void ConvertRotationMatrixToEuler(const int16_t inMatrix[3][3], float& pitch, float& yaw, float& roll);
void ConvertEulerToRotationMatrix(const float pitch, const float yaw, const float roll, int16_t outMatrix[3][3]);

//// External / World-anchored Rotation
void RotateMatrixExternal(const int16_t inMatrix[3][3], Vector3 worldAxis, float angleRad, int16_t outMatrix[3][3]);
void RotateMatrixExternal(const float inMatrix[3][3], Vector3 worldAxis, float angleRad, float outMatrix[3][3]);

}