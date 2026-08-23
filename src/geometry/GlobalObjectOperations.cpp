#include "geometry/GlobalObjectOperations.h"
#include "viewport/LocalGeometry.h"
#include "core/History.h"
#include "formats/IPDParse.h"
#include "formats/IPDWrite.h"
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <filesystem>

namespace Geometry {

static std::string GetWorkspaceDir(const LocalGeometryOverlay& overlay) {
    if (std::filesystem::exists(overlay.m_lastWorkspaceDir))
        return overlay.m_lastWorkspaceDir;
    if (std::filesystem::exists("data/workspace"))
        return "data/workspace";
    if (std::filesystem::exists("../data/workspace"))
        return "../data/workspace";
    return overlay.m_lastWorkspaceDir;
}

static bool GetActiveGlobalObject(LocalGeometryOverlay& overlay,
                                  LoadedChunk*& outChunk,
                                  RenderObject*& outObj) {
    if (overlay.m_selectedChunk.empty() || overlay.m_selectedObjectIdx < 0)
        return false;

    for (auto& lcConst : overlay.GetChunks()) {
        auto& lc = const_cast<LoadedChunk&>(lcConst);
        if (lc.data->chunkName == overlay.m_selectedChunk) {
            if (overlay.m_selectedObjectIdx < (int)lc.data->objects.size()) {
                auto& obj = lc.data->objects[overlay.m_selectedObjectIdx];
                if (obj.isGlobal) {
                    outChunk = &lc;
                    outObj = &obj;
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

static float FindFloorHeightBelow(const LocalGeometryOverlay& overlay, float worldX, float worldZ, float startY, const std::string& ignoreChunk, int ignoreObjIdx) {
    float highestFloorY = -99999.0f;
    Ray ray;
    ray.position = { worldX, startY + 5.0f, worldZ };
    ray.direction = { 0.0f, -1.0f, 0.0f };

    for (const auto& lc : overlay.GetChunks()) {
        if (!lc.visible || lc.hasError) continue;
        for (size_t oi = 0; oi < lc.data->objects.size(); ++oi) {
            if (lc.data->chunkName == ignoreChunk && (int)oi == ignoreObjIdx)
                continue;

            const auto& obj = lc.data->objects[oi];
            if (worldX < obj.bounds.min.x - 0.2f || worldX > obj.bounds.max.x + 0.2f ||
                worldZ < obj.bounds.min.z - 0.2f || worldZ > obj.bounds.max.z + 0.2f)
                continue;

            for (const auto& mesh : obj.meshes) {
                for (const auto& face : mesh.faces) {
                    bool isQuad = (face.v[3] != 0xFF);
                    int triCount = isQuad ? 2 : 1;
                    static const int triV[2][3] = {{0, 1, 2}, {0, 2, 3}};

                    for (int t = 0; t < triCount; ++t) {
                        Vector3 v1 = {mesh.vx[face.v[triV[t][0]]], mesh.vy[face.v[triV[t][0]]], mesh.vz[face.v[triV[t][0]]]};
                        Vector3 v2 = {mesh.vx[face.v[triV[t][1]]], mesh.vy[face.v[triV[t][1]]], mesh.vz[face.v[triV[t][1]]]};
                        Vector3 v3 = {mesh.vx[face.v[triV[t][2]]], mesh.vy[face.v[triV[t][2]]], mesh.vz[face.v[triV[t][2]]]};

                        RayCollision hit = GetRayCollisionTriangle(ray, v1, v2, v3);
                        if (hit.hit && hit.point.y <= startY + 0.1f && hit.point.y > highestFloorY) {
                            highestFloorY = hit.point.y;
                        }
                    }
                }
            }
        }
    }
    return highestFloorY;
}

bool RotateGlobalObject(LocalGeometryOverlay& overlay, int yawSteps, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    if (!GetActiveGlobalObject(overlay, lc, obj))
        return false;

    float angleRad = (float)yawSteps * (PI * 0.5f);
    float cosA = cosf(angleRad);
    float sinA = sinf(angleRad);

    // Calculate world origin of the instance
    Vector3 origin = {
        ((float)obj->rawTx + 10240.0f * (float)lc->data->xPos) * (1.0f / 256.0f),
        -((float)obj->rawTy) * (1.0f / 256.0f),
        -((float)obj->rawTz + 10240.0f * (float)lc->data->yPos) * (1.0f / 256.0f)
    };

    // Before snapshot
    MeshSnapshot snap;
    if (history) {
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = 0;
        if (!obj->meshes.empty()) {
            snap.before = obj->meshes[0];
        }
        snap.description = "Rotate Global Prop " + obj->name;
    }

    // Rotate all vertices in all meshes around origin
    obj->bounds = {{99999.0f, 99999.0f, 99999.0f}, {-99999.0f, -99999.0f, -99999.0f}};
    for (auto& mesh : obj->meshes) {
        for (size_t i = 0; i < mesh.vx.size(); ++i) {
            float dx = mesh.vx[i] - origin.x;
            float dz = mesh.vz[i] - origin.z;

            mesh.vx[i] = origin.x + dx * cosA - dz * sinA;
            mesh.vz[i] = origin.z + dx * sinA + dz * cosA;

            obj->bounds.min.x = std::min(obj->bounds.min.x, mesh.vx[i]);
            obj->bounds.min.y = std::min(obj->bounds.min.y, mesh.vy[i]);
            obj->bounds.min.z = std::min(obj->bounds.min.z, mesh.vz[i]);
            obj->bounds.max.x = std::max(obj->bounds.max.x, mesh.vx[i]);
            obj->bounds.max.y = std::max(obj->bounds.max.y, mesh.vy[i]);
            obj->bounds.max.z = std::max(obj->bounds.max.z, mesh.vz[i]);
        }
    }

    // Update 3x3 rotation matrix (4096 fixed-point)
    // R_new = R_yaw * R_old
    int16_t oldRt[3][3];
    memcpy(oldRt, obj->rt, sizeof(oldRt));
    int c4096 = (int)round(cosA * 4096.0f);
    int s4096 = (int)round(sinA * 4096.0f);

    obj->rt[0][0] = (int16_t)((c4096 * oldRt[0][0] - s4096 * oldRt[2][0]) >> 12);
    obj->rt[0][1] = (int16_t)((c4096 * oldRt[0][1] - s4096 * oldRt[2][1]) >> 12);
    obj->rt[0][2] = (int16_t)((c4096 * oldRt[0][2] - s4096 * oldRt[2][2]) >> 12);

    obj->rt[2][0] = (int16_t)((s4096 * oldRt[0][0] + c4096 * oldRt[2][0]) >> 12);
    obj->rt[2][1] = (int16_t)((s4096 * oldRt[0][1] + c4096 * oldRt[2][1]) >> 12);
    obj->rt[2][2] = (int16_t)((s4096 * oldRt[0][2] + c4096 * oldRt[2][2]) >> 12);

    if (history && !obj->meshes.empty()) {
        snap.after = obj->meshes[0];
        history->Push(std::move(snap));
    }

    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool SnapGlobalObjectToFloor(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    if (!GetActiveGlobalObject(overlay, lc, obj))
        return false;

    float minY = 99999.0f;
    for (const auto& mesh : obj->meshes) {
        for (float y : mesh.vy) {
            if (y < minY) minY = y;
        }
    }
    if (minY > 90000.0f) return false;

    float centerX = (obj->bounds.min.x + obj->bounds.max.x) * 0.5f;
    float centerZ = (obj->bounds.min.z + obj->bounds.max.z) * 0.5f;
    float targetFloorY = FindFloorHeightBelow(overlay, centerX, centerZ, minY, lc->data->chunkName, overlay.m_selectedObjectIdx);

    // Test bounding box corners if center missed
    if (targetFloorY < -90000.0f) {
        float testPts[4][2] = {
            {obj->bounds.min.x, obj->bounds.min.z},
            {obj->bounds.max.x, obj->bounds.min.z},
            {obj->bounds.min.x, obj->bounds.max.z},
            {obj->bounds.max.x, obj->bounds.max.z}
        };
        for (int p = 0; p < 4; ++p) {
            float hy = FindFloorHeightBelow(overlay, testPts[p][0], testPts[p][1], minY, lc->data->chunkName, overlay.m_selectedObjectIdx);
            if (hy > targetFloorY) targetFloorY = hy;
        }
    }

    if (targetFloorY < -90000.0f) {
        targetFloorY = 0.0f; // fallback to level ground
    }

    float deltaY = targetFloorY - minY;
    if (fabsf(deltaY) < 0.0001f) return false;

    MeshSnapshot snap;
    if (history) {
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = 0;
        if (!obj->meshes.empty()) snap.before = obj->meshes[0];
        snap.description = "Snap Prop " + obj->name + " to Floor";
    }

    // Offset vertices and rawTy
    int32_t rawDy = (int32_t)round(deltaY * 256.0f);
    obj->rawTy -= rawDy;

    for (auto& mesh : obj->meshes) {
        for (float& y : mesh.vy) {
            y += deltaY;
        }
    }
    obj->bounds.min.y += deltaY;
    obj->bounds.max.y += deltaY;

    if (history && !obj->meshes.empty()) {
        snap.after = obj->meshes[0];
        history->Push(std::move(snap));
    }

    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool SnapGlobalObjectToGrid(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    if (!GetActiveGlobalObject(overlay, lc, obj))
        return false;

    int rawStep = 1 << overlay.m_moveStepPower;
    int32_t newRawTx = (int32_t)round((double)obj->rawTx / (double)rawStep) * rawStep;
    int32_t newRawTz = (int32_t)round((double)obj->rawTz / (double)rawStep) * rawStep;

    int32_t dTx = newRawTx - obj->rawTx;
    int32_t dTz = newRawTz - obj->rawTz;
    if (dTx == 0 && dTz == 0) return false;

    float dx = (float)dTx * (1.0f / 256.0f);
    float dz = -(float)dTz * (1.0f / 256.0f);

    MeshSnapshot snap;
    if (history) {
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = 0;
        if (!obj->meshes.empty()) snap.before = obj->meshes[0];
        snap.description = "Snap Prop " + obj->name + " to Grid";
    }

    obj->rawTx = newRawTx;
    obj->rawTz = newRawTz;

    for (auto& mesh : obj->meshes) {
        for (size_t i = 0; i < mesh.vx.size(); ++i) {
            mesh.vx[i] += dx;
            mesh.vz[i] += dz;
        }
    }
    obj->bounds.min.x += dx; obj->bounds.max.x += dx;
    obj->bounds.min.z += dz; obj->bounds.max.z += dz;

    if (history && !obj->meshes.empty()) {
        snap.after = obj->meshes[0];
        history->Push(std::move(snap));
    }

    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool DuplicateGlobalObject(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    if (!GetActiveGlobalObject(overlay, lc, obj))
        return false;

    RenderObject newObj = *obj;
    newObj.ipdDataOffset = -1; // Must be -1 to allocate a new IPD_OBJ_DATA entry on write
    newObj.ipdObjId = obj->ipdObjId;
    newObj.ipdPosGroup = obj->ipdPosGroup;

    // Offset by +1.0m (256 raw units) along X
    newObj.rawTx += 256;
    for (auto& mesh : newObj.meshes) {
        for (float& x : mesh.vx) {
            x += 1.0f;
        }
    }
    newObj.bounds.min.x += 1.0f;
    newObj.bounds.max.x += 1.0f;

    lc->data->objects.push_back(std::move(newObj));
    overlay.m_selectedObjectIdx = (int)lc->data->objects.size() - 1;

    if (history) {
        MeshSnapshot snap;
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = 0;
        if (!lc->data->objects[overlay.m_selectedObjectIdx].meshes.empty()) {
            snap.after = lc->data->objects[overlay.m_selectedObjectIdx].meshes[0];
        }
        snap.description = "Duplicate Global Prop " + obj->name;
        history->Push(std::move(snap));
    }

    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool DeleteGlobalObject(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    if (!GetActiveGlobalObject(overlay, lc, obj))
        return false;

    if (history) {
        MeshSnapshot snap;
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = 0;
        if (!obj->meshes.empty()) snap.before = obj->meshes[0];
        snap.description = "Delete Global Prop " + obj->name;
        history->Push(std::move(snap));
    }

    lc->data->objects.erase(lc->data->objects.begin() + overlay.m_selectedObjectIdx);
    overlay.m_selectedObjectIdx = -1;

    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool MirrorGlobalObject(LocalGeometryOverlay& overlay, int axis, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    if (!GetActiveGlobalObject(overlay, lc, obj))
        return false;

    Vector3 center = {
        (obj->bounds.min.x + obj->bounds.max.x) * 0.5f,
        (obj->bounds.min.y + obj->bounds.max.y) * 0.5f,
        (obj->bounds.min.z + obj->bounds.max.z) * 0.5f
    };

    MeshSnapshot snap;
    if (history) {
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = 0;
        if (!obj->meshes.empty()) snap.before = obj->meshes[0];
        snap.description = "Mirror Prop " + obj->name;
    }

    obj->bounds = {{99999.0f, 99999.0f, 99999.0f}, {-99999.0f, -99999.0f, -99999.0f}};
    for (auto& mesh : obj->meshes) {
        for (size_t i = 0; i < mesh.vx.size(); ++i) {
            if (axis == 0) mesh.vx[i] = 2.0f * center.x - mesh.vx[i];
            else if (axis == 1) mesh.vy[i] = 2.0f * center.y - mesh.vy[i];
            else if (axis == 2) mesh.vz[i] = 2.0f * center.z - mesh.vz[i];

            obj->bounds.min.x = std::min(obj->bounds.min.x, mesh.vx[i]);
            obj->bounds.min.y = std::min(obj->bounds.min.y, mesh.vy[i]);
            obj->bounds.min.z = std::min(obj->bounds.min.z, mesh.vz[i]);
            obj->bounds.max.x = std::max(obj->bounds.max.x, mesh.vx[i]);
            obj->bounds.max.y = std::max(obj->bounds.max.y, mesh.vy[i]);
            obj->bounds.max.z = std::max(obj->bounds.max.z, mesh.vz[i]);
        }

        // Invert face winding
        for (auto& face : mesh.faces) {
            bool isQuad = (face.v[3] != 0xFF);
            if (isQuad) {
                std::swap(face.v[1], face.v[3]);
                std::swap(face.uv[1][0], face.uv[3][0]);
                std::swap(face.uv[1][1], face.uv[3][1]);
            } else {
                std::swap(face.v[0], face.v[2]);
                std::swap(face.uv[0][0], face.uv[2][0]);
                std::swap(face.uv[0][1], face.uv[2][1]);
            }
        }
    }

    if (history && !obj->meshes.empty()) {
        snap.after = obj->meshes[0];
        history->Push(std::move(snap));
    }

    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool MoveGlobalObjectToChunk(LocalGeometryOverlay& overlay, const std::string& targetChunkName, History* history) {
    if (targetChunkName == overlay.m_selectedChunk || targetChunkName.empty())
        return false;

    LoadedChunk* srcLc = nullptr;
    RenderObject* srcObj = nullptr;
    if (!GetActiveGlobalObject(overlay, srcLc, srcObj))
        return false;

    LoadedChunk* dstLc = nullptr;
    for (auto& lcConst : overlay.GetChunks()) {
        auto& lc = const_cast<LoadedChunk&>(lcConst);
        if (lc.data->chunkName == targetChunkName) {
            dstLc = &lc;
            break;
        }
    }
    if (!dstLc) return false;

    // Shift rawTx/rawTz relative to destination chunk cell offset
    int dxCell = srcLc->data->xPos - dstLc->data->xPos;
    int dyCell = srcLc->data->yPos - dstLc->data->yPos;

    RenderObject movedObj = *srcObj;
    movedObj.rawTx += dxCell * 10240;
    movedObj.rawTz += dyCell * 10240;

    // Remove from source and add to dest
    srcLc->data->objects.erase(srcLc->data->objects.begin() + overlay.m_selectedObjectIdx);
    dstLc->data->objects.push_back(std::move(movedObj));

    overlay.m_selectedChunk = targetChunkName;
    overlay.m_selectedObjectIdx = (int)dstLc->data->objects.size() - 1;

    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(srcLc->data->chunkName, ws);
    overlay.RebuildChunkBatches(dstLc->data->chunkName, ws);
    return true;
}

} // namespace Geometry
