#include "geometry/GlobalObjectOperations.h"
#include "geometry/TransformOperations.h"
#include "geometry/GeometryCommon.h"
#include "geometry/ChunkOwnership.h"
#include "viewport/LocalGeometryOverlay.h"
#include "core/History.h"
#include "formats/IPDParse.h"
#include "formats/IPDWrite.h"
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace Geometry {

// --- Private Helpers ---

static bool GetActiveGlobalObject(const LocalGeometryOverlay& overlay,
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

static Vector3 GetGlobalObjectWorldAnchor(const LoadedChunk& lc, const RenderObject& obj) {
    return {
        ((float)obj.rawTx + 10240.0f * (float)lc.data->xPos) * (1.0f / 256.0f),
        -((float)obj.rawTy) * (1.0f / 256.0f),
        -((float)obj.rawTz + 10240.0f * (float)lc.data->yPos) * (1.0f / 256.0f)
    };
}

// Transactional wrapper that manages object resolution, history snapshotting, bounds recalculation, and batch rebuilding
template <typename MutateFn>
static bool ModifyActiveGlobalObject(LocalGeometryOverlay& overlay,
                                     History* history,
                                     const std::string& actionDesc,
                                     MutateFn&& mutate)
{
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    if (!GetActiveGlobalObject(overlay, lc, obj))
        return false;

    MeshSnapshot snap;
    if (history) {
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = 0;
        if (!obj->meshes.empty()) {
            snap.before = obj->meshes[0];
        }
        snap.hasObjectTransform = true;
        snap.rawTxBefore = obj->rawTx;
        snap.rawTyBefore = obj->rawTy;
        snap.rawTzBefore = obj->rawTz;
        memcpy(snap.rtBefore, obj->rt, sizeof(obj->rt));
        snap.description = actionDesc + " " + obj->name;
    }

    if (!mutate(*lc, *obj))
        return false;

    RecalculateObjectBounds(*obj);

    if (history && !obj->meshes.empty()) {
        snap.after = obj->meshes[0];
        snap.rawTxAfter = obj->rawTx;
        snap.rawTyAfter = obj->rawTy;
        snap.rawTzAfter = obj->rawTz;
        memcpy(snap.rtAfter, obj->rt, sizeof(obj->rt));
        history->Push(std::move(snap));
    }

    overlay.RebuildChunkBatches(lc->data->chunkName, GetWorkspaceDir(overlay));
    return true;
}

// --- Public Operations ---

bool GetGlobalObjectRotation(const LocalGeometryOverlay& overlay, float& pitch, float& yaw, float& roll) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    if (!GetActiveGlobalObject(overlay, lc, obj))
        return false;

    ConvertRotationMatrixToEuler(obj->rt, pitch, yaw, roll);
    return true;
}

bool SetGlobalObjectRotation(LocalGeometryOverlay& overlay, float pitch, float yaw, float roll, History* history) {
    return ModifyActiveGlobalObject(overlay, history, "Set Prop Rotation", [&](LoadedChunk& lc, RenderObject& obj) {
        // Calculate new engine matrix and corresponding float matrix
        int16_t targetEngineMat[3][3];
        ConvertEulerToRotationMatrix(pitch, yaw, roll, targetEngineMat);

        float targetMat[3][3];
        RescaleEngineToViewport(targetEngineMat, targetMat);

        // Convert current engine matrix to float
        float currentMat[3][3];
        RescaleEngineToViewport(obj.rt, currentMat);

        // Delta rotation = targetMat * transpose(currentMat)
        float currentTransposed[3][3];
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                currentTransposed[r][c] = currentMat[c][r];
            }
        }

        float deltaRot[3][3];
        Matrix3x3Multiply(targetMat, currentTransposed, deltaRot);

        // Transform vertices around instance world anchor
        Vector3 anchor = GetGlobalObjectWorldAnchor(lc, obj);
        for (auto& mesh : obj.meshes) {
            for (size_t i = 0; i < mesh.vx.size(); ++i) {
                Vector3 p = { mesh.vx[i], mesh.vy[i], mesh.vz[i] };
                Vector3 pNew = TransformPointAroundPivot(p, anchor, deltaRot);
                mesh.vx[i] = pNew.x;
                mesh.vy[i] = pNew.y;
                mesh.vz[i] = pNew.z;
            }
        }

        memcpy(obj.rt, targetEngineMat, sizeof(obj.rt));
        return true;
    });
}

bool RotateGlobalObjectExternal(LocalGeometryOverlay& overlay, Vector3 worldAxis, float angleRad, History* history) {
    return ModifyActiveGlobalObject(overlay, history, "Rotate Prop", [&](LoadedChunk& lc, RenderObject& obj) {
        float deltaRot[3][3];
        CreateAxisAngleMatrix(worldAxis, angleRad, deltaRot);

        Vector3 anchor = GetGlobalObjectWorldAnchor(lc, obj);
        for (auto& mesh : obj.meshes) {
            for (size_t i = 0; i < mesh.vx.size(); ++i) {
                Vector3 p = { mesh.vx[i], mesh.vy[i], mesh.vz[i] };
                Vector3 pNew = TransformPointAroundPivot(p, anchor, deltaRot);
                mesh.vx[i] = pNew.x;
                mesh.vy[i] = pNew.y;
                mesh.vz[i] = pNew.z;
            }
        }

        RotateMatrixExternal(obj.rt, worldAxis, angleRad, obj.rt);
        return true;
    });
}

bool RotateGlobalObject(LocalGeometryOverlay& overlay, int yawSteps, History* history) {
    return RotateGlobalObjectExternal(overlay, { 0.0f, 1.0f, 0.0f }, (float)yawSteps * (PI * 0.5f), history);
}

bool TranslateGlobalObject(LocalGeometryOverlay& overlay, Vector3 worldDelta, History* history) {
    return ModifyActiveGlobalObject(overlay, history, "Translate Prop", [&](LoadedChunk&, RenderObject& obj) {
        int32_t dTx = (int32_t)round(worldDelta.x * 256.0f);
        int32_t dTy = -(int32_t)round(worldDelta.y * 256.0f);
        int32_t dTz = -(int32_t)round(worldDelta.z * 256.0f);

        obj.rawTx += dTx;
        obj.rawTy += dTy;
        obj.rawTz += dTz;

        for (auto& mesh : obj.meshes) {
            for (size_t i = 0; i < mesh.vx.size(); ++i) {
                mesh.vx[i] += worldDelta.x;
                mesh.vy[i] += worldDelta.y;
                mesh.vz[i] += worldDelta.z;
            }
        }
        return true;
    });
}

bool SnapGlobalObjectToFloor(LocalGeometryOverlay& overlay, History* history) {
    return ModifyActiveGlobalObject(overlay, history, "Snap Prop to Floor", [&](LoadedChunk& lc, RenderObject& obj) {
        float minY = 99999.0f;
        for (const auto& mesh : obj.meshes) {
            for (float y : mesh.vy) {
                if (y < minY) minY = y;
            }
        }
        if (minY > 90000.0f) return false;

        float centerX = (obj.bounds.min.x + obj.bounds.max.x) * 0.5f;
        float centerZ = (obj.bounds.min.z + obj.bounds.max.z) * 0.5f;
        float targetFloorY = FindFloorHeightBelow(overlay, centerX, centerZ, minY, lc.data->chunkName, overlay.m_selectedObjectIdx);

        if (targetFloorY < -90000.0f) {
            float testPts[4][2] = {
                {obj.bounds.min.x, obj.bounds.min.z},
                {obj.bounds.max.x, obj.bounds.min.z},
                {obj.bounds.min.x, obj.bounds.max.z},
                {obj.bounds.max.x, obj.bounds.max.z}
            };
            for (int p = 0; p < 4; ++p) {
                float hy = FindFloorHeightBelow(overlay, testPts[p][0], testPts[p][1], minY, lc.data->chunkName, overlay.m_selectedObjectIdx);
                if (hy > targetFloorY) targetFloorY = hy;
            }
        }

        if (targetFloorY < -90000.0f) {
            targetFloorY = 0.0f;
        }

        float deltaY = targetFloorY - minY;
        if (fabsf(deltaY) < 0.0001f) return false;

        int32_t rawDy = (int32_t)round(deltaY * 256.0f);
        obj.rawTy -= rawDy;

        for (auto& mesh : obj.meshes) {
            for (float& y : mesh.vy) {
                y += deltaY;
            }
        }
        return true;
    });
}

bool SnapGlobalObjectToGrid(LocalGeometryOverlay& overlay, History* history) {
    return ModifyActiveGlobalObject(overlay, history, "Snap Prop to Grid", [&](LoadedChunk&, RenderObject& obj) {
        int rawStep = 1 << overlay.m_moveStepPower;
        int32_t newRawTx = (int32_t)round((double)obj.rawTx / (double)rawStep) * rawStep;
        int32_t newRawTz = (int32_t)round((double)obj.rawTz / (double)rawStep) * rawStep;

        int32_t dTx = newRawTx - obj.rawTx;
        int32_t dTz = newRawTz - obj.rawTz;
        if (dTx == 0 && dTz == 0) return false;

        float dx = (float)dTx * (1.0f / 256.0f);
        float dz = -(float)dTz * (1.0f / 256.0f);

        obj.rawTx = newRawTx;
        obj.rawTz = newRawTz;

        for (auto& mesh : obj.meshes) {
            for (size_t i = 0; i < mesh.vx.size(); ++i) {
                mesh.vx[i] += dx;
                mesh.vz[i] += dz;
            }
        }
        return true;
    });
}

bool DuplicateGlobalObject(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    if (!GetActiveGlobalObject(overlay, lc, obj))
        return false;

    RenderObject newObj = *obj;
    newObj.ipdDataOffset = -1; // Allocates a new IPD_OBJ_DATA entry on write
    newObj.ipdObjId = obj->ipdObjId;
    newObj.ipdPosGroup = obj->ipdPosGroup;

    // Offset by +1.0m (256 raw units) along X
    newObj.rawTx += 256;
    for (auto& mesh : newObj.meshes) {
        for (float& x : mesh.vx) {
            x += 1.0f;
        }
    }
    RecalculateObjectBounds(newObj);

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

    overlay.RebuildChunkBatches(lc->data->chunkName, GetWorkspaceDir(overlay));
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

    overlay.RebuildChunkBatches(lc->data->chunkName, GetWorkspaceDir(overlay));
    return true;
}

bool MirrorGlobalObject(LocalGeometryOverlay& overlay, int axis, History* history) {
    return ModifyActiveGlobalObject(overlay, history, "Mirror Prop", [&](LoadedChunk&, RenderObject& obj) {
        Vector3 center = {
            (obj.bounds.min.x + obj.bounds.max.x) * 0.5f,
            (obj.bounds.min.y + obj.bounds.max.y) * 0.5f,
            (obj.bounds.min.z + obj.bounds.max.z) * 0.5f
        };

        for (auto& mesh : obj.meshes) {
            for (size_t i = 0; i < mesh.vx.size(); ++i) {
                if (axis == 0) mesh.vx[i] = 2.0f * center.x - mesh.vx[i];
                else if (axis == 1) mesh.vy[i] = 2.0f * center.y - mesh.vy[i];
                else if (axis == 2) mesh.vz[i] = 2.0f * center.z - mesh.vz[i];
            }

            // Invert face winding
            for (auto& face : mesh.faces) {
                InvertPolygonWinding(face.v, face.uv, face.rawU, face.rawV);
            }
        }
        return true;
    });
}

} // namespace Geometry
