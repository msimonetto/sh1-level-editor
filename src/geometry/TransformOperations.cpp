// Engine stores rotations as 3D matrix (Q12 integers)
// UI uses Euler angles (stored as float)

#include "geometry/TransformOperations.h"
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace Geometry {

// 3D Vector & Normal utilities
Vector3 ComputeTriangleNormal(Vector3 v0, Vector3 v1, Vector3 v2, Vector3 fallback) {
    Vector3 e1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
    Vector3 e2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };
    Vector3 cross = {
        e1.y * e2.z - e1.z * e2.y,
        e1.z * e2.x - e1.x * e2.z,
        e1.x * e2.y - e1.y * e2.x
    };
    float lenSq = cross.x * cross.x + cross.y * cross.y + cross.z * cross.z;
    if (lenSq < 1e-8f) return fallback;
    float invLen = 1.0f / sqrtf(lenSq);
    return { cross.x * invLen, cross.y * invLen, cross.z * invLen };
}

// UV & Polygon utilities
uint8_t NormalizedToByteUv(float uv) {
    return (uint8_t)std::clamp((int)std::lroundf(uv * 255.0f), 0, 255);
}

float ByteToNormalizedUv(uint8_t raw) {
    return (float)raw * (1.0f / 255.0f);
}

void ComputeUvBounds(const float uv[4][2], int numVerts, float& minU, float& maxU, float& minV, float& maxV) {
    minU = 9999.0f; maxU = -9999.0f;
    minV = 9999.0f; maxV = -9999.0f;
    for (int i = 0; i < numVerts; ++i) {
        minU = std::min(minU, uv[i][0]);
        maxU = std::max(maxU, uv[i][0]);
        minV = std::min(minV, uv[i][1]);
        maxV = std::max(maxV, uv[i][1]);
    }
}

void InvertPolygonWinding(uint8_t v[4], float uv[4][2], uint8_t rawU[4], uint8_t rawV[4]) {
    bool isQuad = (v[3] != 0xFF);
    if (isQuad) {
        std::swap(v[1], v[3]);
        std::swap(uv[1][0], uv[3][0]);
        std::swap(uv[1][1], uv[3][1]);
        std::swap(rawU[1], rawU[3]);
        std::swap(rawV[1], rawV[3]);
    } else {
        std::swap(v[0], v[2]);
        std::swap(uv[0][0], uv[2][0]);
        std::swap(uv[0][1], uv[2][1]);
        std::swap(rawU[0], rawU[2]);
        std::swap(rawV[0], rawV[2]);
    }
}

void RotatePolygonUv(float uv[4][2], uint8_t rawU[4], uint8_t rawV[4], int numVerts, int steps) {
    if (numVerts < 3) return;
    int actualSteps = steps % numVerts;
    if (actualSteps < 0) actualSteps += numVerts;

    for (int s = 0; s < actualSteps; ++s) {
        float lastU = uv[numVerts - 1][0];
        float lastV = uv[numVerts - 1][1];
        uint8_t lastRawU = rawU[numVerts - 1];
        uint8_t lastRawV = rawV[numVerts - 1];

        for (int i = numVerts - 1; i > 0; --i) {
            uv[i][0] = uv[i - 1][0];
            uv[i][1] = uv[i - 1][1];
            rawU[i] = rawU[i - 1];
            rawV[i] = rawV[i - 1];
        }
        uv[0][0] = lastU;
        uv[0][1] = lastV;
        rawU[0] = lastRawU;
        rawV[0] = lastRawV;
    }
}

void ResetFaceDefaultUV(float uv[4][2], uint8_t rawU[4], uint8_t rawV[4], int numVerts) {
    if (numVerts >= 4) {
        uv[0][0] = 0.0f; uv[0][1] = 0.0f;
        uv[1][0] = 1.0f; uv[1][1] = 0.0f;
        uv[2][0] = 1.0f; uv[2][1] = 1.0f;
        uv[3][0] = 0.0f; uv[3][1] = 1.0f;

        rawU[0] = 0; rawU[1] = 255; rawU[2] = 255; rawU[3] = 0;
        rawV[0] = 0; rawV[1] = 0;   rawV[2] = 255; rawV[3] = 255;
    } else {
        uv[0][0] = 0.0f; uv[0][1] = 0.0f;
        uv[1][0] = 1.0f; uv[1][1] = 0.0f;
        uv[2][0] = 1.0f; uv[2][1] = 1.0f;

        rawU[0] = 0; rawU[1] = 255; rawU[2] = 255;
        rawV[0] = 0; rawV[1] = 0;   rawV[2] = 255;
    }
}

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