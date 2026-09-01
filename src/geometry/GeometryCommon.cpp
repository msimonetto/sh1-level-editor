#include "geometry/GeometryCommon.h"
#include "geometry/MeshOperations.h"
#include "geometry/TransformOperations.h"
#include "viewport/LocalGeometryOverlay.h"
#include "formats/IPDParse.h"
#include "core/History.h"
#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <tuple>

namespace Geometry {

std::string GetWorkspaceDir(const LocalGeometryOverlay& overlay) {
    if (std::filesystem::exists(overlay.m_lastWorkspaceDir))
        return overlay.m_lastWorkspaceDir;
    if (std::filesystem::exists("data/workspace"))
        return "data/workspace";
    if (std::filesystem::exists("../data/workspace"))
        return "../data/workspace";
    return overlay.m_lastWorkspaceDir;
}

float FindFloorHeightBelow(const LocalGeometryOverlay& overlay,
                           float worldX,
                           float worldZ,
                           float startY,
                           const std::string& ignoreChunk,
                           int ignoreObjIdx) {
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

void RecalculateObjectBounds(RenderObject& obj) {
    obj.bounds = {{99999.0f, 99999.0f, 99999.0f}, {-99999.0f, -99999.0f, -99999.0f}};
    for (const auto& mesh : obj.meshes) {
        for (size_t i = 0; i < mesh.vx.size(); ++i) {
            obj.bounds.min.x = std::min(obj.bounds.min.x, mesh.vx[i]);
            obj.bounds.min.y = std::min(obj.bounds.min.y, mesh.vy[i]);
            obj.bounds.min.z = std::min(obj.bounds.min.z, mesh.vz[i]);
            obj.bounds.max.x = std::max(obj.bounds.max.x, mesh.vx[i]);
            obj.bounds.max.y = std::max(obj.bounds.max.y, mesh.vy[i]);
            obj.bounds.max.z = std::max(obj.bounds.max.z, mesh.vz[i]);
        }
    }
}

std::set<SelectedVertex> GetEffectiveSelectedVertices(const LocalGeometryOverlay& overlay) {
    std::set<SelectedVertex> result = overlay.m_selectedVertices;
    if (result.empty() && overlay.m_selectedVertexIdx >= 0 && !overlay.m_selectedChunk.empty() && overlay.m_selectedObjectIdx >= 0) {
        result.insert({overlay.m_selectedChunk, overlay.m_selectedObjectIdx, overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0, overlay.m_selectedVertexIdx});
    }
    return result;
}

std::set<SelectedFace> GetEffectiveSelectedFaces(const LocalGeometryOverlay& overlay) {
    std::set<SelectedFace> result = overlay.m_selectedFaces;
    if (result.empty() && overlay.m_selectedFaceIdx >= 0 && !overlay.m_selectedChunk.empty() && overlay.m_selectedObjectIdx >= 0) {
        result.insert({overlay.m_selectedChunk, overlay.m_selectedObjectIdx, overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0, overlay.m_selectedFaceIdx});
    }
    return result;
}

void CompactMeshUnusedVertices(RenderMesh& mesh) {
    size_t numV = mesh.vx.size();
    std::vector<bool> vertUsed(numV, false);
    for (const auto& f : mesh.faces) {
        if (f.v[0] < numV) vertUsed[f.v[0]] = true;
        if (f.v[1] < numV) vertUsed[f.v[1]] = true;
        if (f.v[2] < numV) vertUsed[f.v[2]] = true;
        if (f.v[3] != 0xFF && f.v[3] < numV) vertUsed[f.v[3]] = true;
    }

    std::vector<float> newVx, newVy, newVz;
    std::vector<int> remap(numV, -1);
    for (size_t i = 0; i < numV; ++i) {
        if (vertUsed[i]) {
            remap[i] = (int)newVx.size();
            newVx.push_back(mesh.vx[i]);
            newVy.push_back(mesh.vy[i]);
            newVz.push_back(mesh.vz[i]);
        }
    }

    mesh.vx = std::move(newVx);
    mesh.vy = std::move(newVy);
    mesh.vz = std::move(newVz);

    for (auto& f : mesh.faces) {
        if (f.v[0] < numV && remap[f.v[0]] >= 0) f.v[0] = (uint8_t)remap[f.v[0]];
        if (f.v[1] < numV && remap[f.v[1]] >= 0) f.v[1] = (uint8_t)remap[f.v[1]];
        if (f.v[2] < numV && remap[f.v[2]] >= 0) f.v[2] = (uint8_t)remap[f.v[2]];
        if (f.v[3] != 0xFF && f.v[3] < numV && remap[f.v[3]] >= 0) {
            f.v[3] = (uint8_t)remap[f.v[3]];
        }
    }
}

bool ForEachSelectedMeshVertices(
    LocalGeometryOverlay& overlay,
    History* history,
    const char* actionDescription,
    const VertexMeshOp& op)
{
    auto selVerts = GetEffectiveSelectedVertices(overlay);
    if (selVerts.empty()) return false;

    std::set<std::tuple<std::string, int, int>> modifiedMeshes;
    for (const auto& sv : selVerts) {
        modifiedMeshes.insert({sv.chunkName, sv.objectIdx, sv.meshIdx});
    }

    bool anyModified = false;

    for (const auto& mRef : modifiedMeshes) {
        const std::string& cName = std::get<0>(mRef);
        int oIdx = std::get<1>(mRef);
        int mIdx = std::get<2>(mRef);

        for (const auto& lcConst : overlay.GetChunks()) {
            auto& lc = const_cast<LoadedChunk&>(lcConst);
            if (lc.data->chunkName != cName || oIdx >= (int)lc.data->objects.size()) continue;
            auto& obj = lc.data->objects[oIdx];
            if (mIdx >= (int)obj.meshes.size()) break;
            auto& mesh = obj.meshes[mIdx];

            std::vector<int> vertIndices;
            for (const auto& sv : selVerts) {
                if (sv.chunkName == cName && sv.objectIdx == oIdx && sv.meshIdx == mIdx)
                    vertIndices.push_back(sv.vertexIdx);
            }
            if (vertIndices.empty()) continue;

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = actionDescription ? actionDescription : "Modify Vertices";
            }

            bool meshChanged = op(lc, obj, mesh, vertIndices);

            if (meshChanged) {
                if (history) {
                    snap.after = mesh;
                    history->Push(std::move(snap));
                }

                RecalculateBounds(overlay);
                std::string ws = GetWorkspaceDir(overlay);
                overlay.RebuildChunkBatches(cName, ws);
                anyModified = true;
            }
        }
    }
    return anyModified;
}

bool GetActiveMeshTarget(const LocalGeometryOverlay& overlay,
                         LoadedChunk*& outLc,
                         RenderObject*& outObj,
                         RenderMesh*& outMesh) {
    outLc = nullptr;
    outObj = nullptr;
    outMesh = nullptr;
    if (overlay.m_selectedChunk.empty() || overlay.m_selectedObjectIdx < 0) return false;

    for (const auto& lcConst : overlay.GetChunks()) {
        auto& lc = const_cast<LoadedChunk&>(lcConst);
        if (lc.data->chunkName == overlay.m_selectedChunk) {
            if (overlay.m_selectedObjectIdx < (int)lc.data->objects.size()) {
                outLc = &lc;
                outObj = &lc.data->objects[overlay.m_selectedObjectIdx];
                int mIdx = overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0;
                if (mIdx < (int)outObj->meshes.size()) {
                    outMesh = &outObj->meshes[mIdx];
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

Vector3 GetMeshVertex(const RenderMesh& mesh, size_t idx) {
    if (idx >= mesh.vx.size()) return { 0.0f, 0.0f, 0.0f };
    return { mesh.vx[idx], mesh.vy[idx], mesh.vz[idx] };
}

uint8_t AddMeshVertex(RenderMesh& mesh, Vector3 pos) {
    uint8_t idx = (uint8_t)mesh.vx.size();
    mesh.vx.push_back(pos.x);
    mesh.vy.push_back(pos.y);
    mesh.vz.push_back(pos.z);
    return idx;
}

Vector3 ComputeVertexCentroid(const RenderMesh& mesh, const std::vector<int>& indices) {
    if (indices.empty()) return { 0.0f, 0.0f, 0.0f };
    Vector3 sum = { 0.0f, 0.0f, 0.0f };
    for (int vi : indices) {
        if (vi >= 0 && vi < (int)mesh.vx.size()) {
            sum.x += mesh.vx[vi];
            sum.y += mesh.vy[vi];
            sum.z += mesh.vz[vi];
        }
    }
    float inv = 1.0f / (float)indices.size();
    return { sum.x * inv, sum.y * inv, sum.z * inv };
}

std::vector<uint8_t> SortVerticesByAngle(const RenderMesh& mesh, const std::vector<int>& vertIndices) {
    if (vertIndices.size() < 3) {
        std::vector<uint8_t> res;
        for (int vi : vertIndices) res.push_back((uint8_t)vi);
        return res;
    }

    Vector3 center = ComputeVertexCentroid(mesh, vertIndices);
    Vector3 v0 = GetMeshVertex(mesh, vertIndices[0]);
    Vector3 v1 = GetMeshVertex(mesh, vertIndices[1]);
    Vector3 v2 = GetMeshVertex(mesh, vertIndices[2]);
    Vector3 norm = ComputeTriangleNormal(v0, v1, v2);

    Vector3 axisU = Vector3Normalize(Vector3Subtract(v0, center));
    if (Vector3Length(axisU) < 0.001f) axisU = { 1.0f, 0.0f, 0.0f };
    Vector3 axisV = Vector3CrossProduct(norm, axisU);

    struct AngleSort {
        uint8_t vIdx;
        float angle;
    };
    std::vector<AngleSort> sorted;
    for (int vi : vertIndices) {
        Vector3 toV = Vector3Subtract(GetMeshVertex(mesh, vi), center);
        float u = Vector3DotProduct(toV, axisU);
        float v = Vector3DotProduct(toV, axisV);
        float ang = std::atan2(v, u);
        sorted.push_back({ (uint8_t)vi, ang });
    }
    std::sort(sorted.begin(), sorted.end(), [](const AngleSort& a, const AngleSort& b) {
        return a.angle < b.angle;
    });

    std::vector<uint8_t> result;
    for (const auto& item : sorted) result.push_back(item.vIdx);
    return result;
}

RenderFace CreateDefaultFace(const RenderObject& obj,
                             int meshIdx,
                             const std::vector<uint8_t>& vertIndices,
                             const RenderFace* inheritFace) {
    RenderFace face;
    std::memset(&face, 0, sizeof(face));
    face.addr.plmObjectName = obj.name;
    face.addr.meshIdx = meshIdx;
    face.addr.isGlobal = obj.isGlobal;

    if (inheritFace && inheritFace->texNum != 0x7F && !inheritFace->texName.empty()) {
        face.texName = inheritFace->texName;
        face.texNum = inheritFace->texNum;
        face.paletteRow = inheritFace->paletteRow;
        face.cbaRaw = inheritFace->cbaRaw;
    } else {
        face.texNum = 0x7F;
    }

    size_t count = vertIndices.size();
    face.v[0] = count > 0 ? vertIndices[0] : 0;
    face.v[1] = count > 1 ? vertIndices[1] : 0;
    face.v[2] = count > 2 ? vertIndices[2] : 0;
    face.v[3] = count > 3 ? vertIndices[3] : 0xFF;

    if (count == 3) {
        face.uv[0][0] = 0.0f; face.uv[0][1] = 0.0f;
        face.uv[1][0] = 1.0f; face.uv[1][1] = 0.0f;
        face.uv[2][0] = 1.0f; face.uv[2][1] = 1.0f;

        face.rawU[0] = 0; face.rawU[1] = 255; face.rawU[2] = 255;
        face.rawV[0] = 0; face.rawV[1] = 0;   face.rawV[2] = 255;
    } else {
        face.uv[0][0] = 0.0f; face.uv[0][1] = 0.0f;
        face.uv[1][0] = 1.0f; face.uv[1][1] = 0.0f;
        face.uv[2][0] = 1.0f; face.uv[2][1] = 1.0f;
        face.uv[3][0] = 0.0f; face.uv[3][1] = 1.0f;

        face.rawU[0] = 0; face.rawU[1] = 255; face.rawU[2] = 255; face.rawU[3] = 0;
        face.rawV[0] = 0; face.rawV[1] = 0;   face.rawV[2] = 255; face.rawV[3] = 255;
    }

    return face;
}

Vector3 ComputeFaceNormal(const RenderMesh& mesh, const RenderFace& face) {
    Vector3 v0 = GetMeshVertex(mesh, face.v[0]);
    Vector3 v1 = GetMeshVertex(mesh, face.v[1]);
    Vector3 v2 = GetMeshVertex(mesh, face.v[2]);
    return ComputeTriangleNormal(v0, v1, v2);
}

bool ForEachSelectedMeshFaces(
    LocalGeometryOverlay& overlay,
    History* history,
    const char* actionDescription,
    const FaceMeshOp& op,
    bool clearSelection,
    bool recalculateBounds)
{
    auto selFaces = GetEffectiveSelectedFaces(overlay);
    if (selFaces.empty()) return false;

    std::set<std::tuple<std::string, int, int>> modifiedMeshes;
    for (const auto& sf : selFaces) {
        modifiedMeshes.insert({sf.chunkName, sf.objectIdx, sf.meshIdx});
    }

    bool anyModified = false;

    for (const auto& mRef : modifiedMeshes) {
        const std::string& cName = std::get<0>(mRef);
        int oIdx = std::get<1>(mRef);
        int mIdx = std::get<2>(mRef);

        for (const auto& lcConst : overlay.GetChunks()) {
            auto& lc = const_cast<LoadedChunk&>(lcConst);
            if (lc.data->chunkName != cName || oIdx >= (int)lc.data->objects.size()) continue;
            auto& obj = lc.data->objects[oIdx];
            if (mIdx >= (int)obj.meshes.size()) break;
            auto& mesh = obj.meshes[mIdx];

            std::vector<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.push_back(sf.faceIdx);
            }
            if (faceIndices.empty()) continue;

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = actionDescription ? actionDescription : "Modify Faces";
            }

            bool meshChanged = op(lc, obj, mesh, faceIndices);

            if (meshChanged) {
                if (history) {
                    snap.after = mesh;
                    history->Push(std::move(snap));
                }

                if (recalculateBounds) {
                    RecalculateBounds(overlay);
                }
                std::string ws = GetWorkspaceDir(overlay);
                overlay.RebuildChunkBatches(cName, ws);
                anyModified = true;
            }
        }
    }

    if (anyModified && clearSelection) {
        auto& mutableOverlay = const_cast<LocalGeometryOverlay&>(overlay);
        mutableOverlay.m_selectedFaces.clear();
        mutableOverlay.m_selectedFaceIdx = -1;
    }

    return anyModified;
}

} // namespace Geometry

