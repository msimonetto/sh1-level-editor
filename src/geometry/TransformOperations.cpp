// Engine stores rotations as 3D matrix (Q12 integers)
// UI uses Euler angles (stored as float)

#include "geometry/TransformOperations.h"
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace Geometry {

// Matrix algebra utilities
void Matrix3x3Multiply(const float a[3][3], const float b[3][3], float out[3][3]) {
    float res[3][3] = {};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            res[r][c] = a[r][0] * b[0][c] + a[r][1] * b[1][c] + a[r][2] * b[2][c];
        }
    }
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            out[r][c] = res[r][c];
        }
    }
}

void CreateAxisAngleMatrix(Vector3 axis, float angleRad, float outMatrix[3][3]) {
    float lenSq = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
    if (lenSq < 1e-8f) {
        outMatrix[0][0] = 1.0f; outMatrix[0][1] = 0.0f; outMatrix[0][2] = 0.0f;
        outMatrix[1][0] = 0.0f; outMatrix[1][1] = 1.0f; outMatrix[1][2] = 0.0f;
        outMatrix[2][0] = 0.0f; outMatrix[2][1] = 0.0f; outMatrix[2][2] = 1.0f;
        return;
    }
    float invLen = 1.0f / sqrtf(lenSq);
    float uX = axis.x * invLen;
    float uY = axis.y * invLen;
    float uZ = axis.z * invLen;

    float cosA = cosf(angleRad);
    float sinA = sinf(angleRad);
    float oneMinusCos = 1.0f - cosA;

    outMatrix[0][0] = cosA + uX * uX * oneMinusCos;
    outMatrix[0][1] = uX * uY * oneMinusCos - uZ * sinA;
    outMatrix[0][2] = uX * uZ * oneMinusCos + uY * sinA;

    outMatrix[1][0] = uY * uX * oneMinusCos + uZ * sinA;
    outMatrix[1][1] = cosA + uY * uY * oneMinusCos;
    outMatrix[1][2] = uY * uZ * oneMinusCos - uX * sinA;

    outMatrix[2][0] = uZ * uX * oneMinusCos - uY * sinA;
    outMatrix[2][1] = uZ * uY * oneMinusCos + uX * sinA;
    outMatrix[2][2] = cosA + uZ * uZ * oneMinusCos;
}

Vector3 TransformPointAroundPivot(Vector3 point, Vector3 pivot, const float rotMatrix[3][3]) {
    float dx = point.x - pivot.x;
    float dy = point.y - pivot.y;
    float dz = point.z - pivot.z;

    return {
        pivot.x + rotMatrix[0][0] * dx + rotMatrix[0][1] * dy + rotMatrix[0][2] * dz,
        pivot.y + rotMatrix[1][0] * dx + rotMatrix[1][1] * dy + rotMatrix[1][2] * dz,
        pivot.z + rotMatrix[2][0] * dx + rotMatrix[2][1] * dy + rotMatrix[2][2] * dz
    };
}

// Rotation
//// Matrix rescaling/reflection: PS1 <-> Viewport
void RescaleEngineToViewport(const int16_t inMatrix[3][3], float outMatrix[3][3]) {
    // Scale from Q12 (PS1) to Float
    // Apply reflections
    outMatrix[0][0] =  inMatrix[0][0] / 4096.0f;
    outMatrix[0][1] = -inMatrix[0][1] / 4096.0f;
    outMatrix[0][2] = -inMatrix[0][2] / 4096.0f;

    outMatrix[1][0] = -inMatrix[1][0] / 4096.0f;
    outMatrix[1][1] =  inMatrix[1][1] / 4096.0f;
    outMatrix[1][2] =  inMatrix[1][2] / 4096.0f;

    outMatrix[2][0] = -inMatrix[2][0] / 4096.0f;
    outMatrix[2][1] =  inMatrix[2][1] / 4096.0f;
    outMatrix[2][2] =  inMatrix[2][2] / 4096.0f;
}

void RescaleViewportToEngine(const float inMatrix[3][3], int16_t outMatrix[3][3]) {
    // Scale from Float to Q12 (PS1)
    // Apply reflections
    outMatrix[0][0] = (int16_t)std::round( inMatrix[0][0] * 4096.f);
    outMatrix[0][1] = (int16_t)std::round(-inMatrix[0][1] * 4096.f);
    outMatrix[0][2] = (int16_t)std::round(-inMatrix[0][2] * 4096.f);

    outMatrix[1][0] = (int16_t)std::round(-inMatrix[1][0] * 4096.f);
    outMatrix[1][1] = (int16_t)std::round( inMatrix[1][1] * 4096.f);
    outMatrix[1][2] = (int16_t)std::round( inMatrix[1][2] * 4096.f);

    outMatrix[2][0] = (int16_t)std::round(-inMatrix[2][0] * 4096.f);
    outMatrix[2][1] = (int16_t)std::round( inMatrix[2][1] * 4096.f);
    outMatrix[2][2] = (int16_t)std::round( inMatrix[2][2] * 4096.f);
}

//// Operations
void ConvertRotationMatrixToEuler(const int16_t inMatrix[3][3], float& pitch, float& yaw, float& roll) {
    float rotMatrix[3][3] = {};
    RescaleEngineToViewport(inMatrix, rotMatrix);

    pitch = asinf(std::clamp(-rotMatrix[1][2], -1.0f, 1.0f)) * RadiansToDegrees;
    yaw   = atan2f(rotMatrix[0][2], rotMatrix[2][2]) * RadiansToDegrees;
    roll  = atan2f(rotMatrix[1][0], rotMatrix[1][1]) * RadiansToDegrees;
}

void ConvertEulerToRotationMatrix(const float pitch, const float yaw, const float roll, int16_t outMatrix[3][3]) {
    float pitchR = pitch * DegreesToRadians;
    float yawR   = yaw   * DegreesToRadians;
    float rollR  = roll  * DegreesToRadians;

    float cosPR = cosf(pitchR), sinPR = sinf(pitchR);
    float cosYR = cosf(yawR),   sinYR = sinf(yawR);
    float cosRR = cosf(rollR),  sinRR = sinf(rollR);

    float rotMatrix[3][3] = {
        { cosYR*cosRR + sinYR*sinPR*sinRR, -cosYR*sinRR + sinYR*sinPR*cosRR,  sinYR*cosPR },
        { cosPR*sinRR,                      cosPR*cosRR,                     -sinPR      },
        { -sinYR*cosRR + cosYR*sinPR*sinRR, sinYR*sinRR + cosYR*sinPR*cosRR,  cosYR*cosPR }
    };

    RescaleViewportToEngine(rotMatrix, outMatrix);
}

//// External / World-anchored Rotation
void RotateMatrixExternal(const float inMatrix[3][3], Vector3 worldAxis, float angleRad, float outMatrix[3][3]) {
    float deltaRot[3][3];
    CreateAxisAngleMatrix(worldAxis, angleRad, deltaRot);
    Matrix3x3Multiply(deltaRot, inMatrix, outMatrix);
}

void RotateMatrixExternal(const int16_t inMatrix[3][3], Vector3 worldAxis, float angleRad, int16_t outMatrix[3][3]) {
    float floatMat[3][3];
    RescaleEngineToViewport(inMatrix, floatMat);

    float rotatedMat[3][3];
    RotateMatrixExternal(floatMat, worldAxis, angleRad, rotatedMat);

    RescaleViewportToEngine(rotatedMat, outMatrix);
}

}