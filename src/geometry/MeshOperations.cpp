#include "geometry/MeshOperations.h"
#include "geometry/TransformOperations.h"
#include "geometry/GeometryCommon.h"
#include "geometry/ChunkValidator.h"
#include "core/History.h"
#include "formats/IPDWrite.h"
#include "raymath.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace Geometry {

bool TranslateSelection(
    Vector3 delta,
    EditMode editMode,
    const std::string& selectedChunk,
    int selectedObjectIdx,
    const std::set<SelectedVertex>& selectedVertices,
    const std::set<SelectedFace>& selectedFaces,
    std::vector<LoadedChunk>& chunks,
    bool autoValidate,
    History* history,
    const std::string& workspaceDir
) {
    if (delta.x == 0 && delta.y == 0 && delta.z == 0)
        return false;

    bool modified = false;

    if (editMode == EditMode::Vertex) {
        if (selectedVertices.empty())
            return false;

        std::set<std::tuple<std::string, int, int>> modifiedMeshes;
        for (const auto &sv : selectedVertices) {
            modifiedMeshes.insert({sv.chunkName, sv.objectIdx, sv.meshIdx});
        }

        for (const auto &mRef : modifiedMeshes) {
            const std::string &cName = std::get<0>(mRef);
            int oIdx = std::get<1>(mRef);
            int mIdx = std::get<2>(mRef);

            for (auto &lc : chunks) {
                if (lc.data->chunkName == cName && oIdx < (int)lc.data->objects.size()) {
                    auto &obj = lc.data->objects[oIdx];
                    if (mIdx < (int)obj.meshes.size()) {
                        auto &mesh = obj.meshes[mIdx];

                        MeshSnapshot snap;
                        if (history) {
                            snap.chunkName = cName;
                            snap.objectIdx = oIdx;
                            snap.meshIdx = mIdx;
                            snap.before = mesh;
                            snap.description = "Translate Vertices";
                        }

                        for (const auto &sv : selectedVertices) {
                            if (sv.chunkName == cName && sv.objectIdx == oIdx && sv.meshIdx == mIdx) {
                                mesh.vx[sv.vertexIdx] += delta.x;
                                mesh.vy[sv.vertexIdx] += delta.y;
                                mesh.vz[sv.vertexIdx] += delta.z;

                                if (autoValidate) {
                                    ValidationIssue issue;
                                    Vector3 newPos = {mesh.vx[sv.vertexIdx], mesh.vy[sv.vertexIdx], mesh.vz[sv.vertexIdx]};
                                    if (!ChunkValidator::ValidateVertexPosition(newPos, lc.data->xPos, lc.data->yPos, &issue)) {
                                        printf("[AutoValidate] %s\n", issue.message.c_str());
                                    }
                                }
                            }
                        }

                        if (history) {
                            snap.after = mesh;
                            history->Push(std::move(snap));
                        }
                        modified = true;
                    }
                }
            }
        }
    } else if (editMode == EditMode::Face) {
        if (selectedFaces.empty())
            return false;

        std::set<std::tuple<std::string, int, int>> modifiedMeshes;
        for (const auto &sf : selectedFaces) {
            modifiedMeshes.insert({sf.chunkName, sf.objectIdx, sf.meshIdx});
        }

        for (const auto &mRef : modifiedMeshes) {
            const std::string &cName = std::get<0>(mRef);
            int oIdx = std::get<1>(mRef);
            int mIdx = std::get<2>(mRef);

            for (auto &lc : chunks) {
                if (lc.data->chunkName == cName && oIdx < (int)lc.data->objects.size()) {
                    auto &obj = lc.data->objects[oIdx];
                    if (mIdx < (int)obj.meshes.size()) {
                        auto &mesh = obj.meshes[mIdx];

                        MeshSnapshot snap;
                        if (history) {
                            snap.chunkName = cName;
                            snap.objectIdx = oIdx;
                            snap.meshIdx = mIdx;
                            snap.before = mesh;
                            snap.description = "Translate Faces";
                        }

                        std::set<uint8_t> affectedVerts;
                        for (const auto &sf : selectedFaces) {
                            if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx) {
                                if (sf.faceIdx < (int)mesh.faces.size()) {
                                    const auto &f = mesh.faces[sf.faceIdx];
                                    affectedVerts.insert(f.v[0]);
                                    affectedVerts.insert(f.v[1]);
                                    affectedVerts.insert(f.v[2]);
                                    if (f.v[3] != 0xFF) affectedVerts.insert(f.v[3]);
                                }
                            }
                        }

                        for (uint8_t vi : affectedVerts) {
                            if (vi < mesh.vx.size()) {
                                mesh.vx[vi] += delta.x;
                                mesh.vy[vi] += delta.y;
                                mesh.vz[vi] += delta.z;

                                if (autoValidate) {
                                    ValidationIssue issue;
                                    Vector3 newPos = {mesh.vx[vi], mesh.vy[vi], mesh.vz[vi]};
                                    if (!ChunkValidator::ValidateVertexPosition(newPos, lc.data->xPos, lc.data->yPos, &issue)) {
                                        printf("[AutoValidate] %s\n", issue.message.c_str());
                                    }
                                }
                            }
                        }

                        if (history) {
                            snap.after = mesh;
                            history->Push(std::move(snap));
                        }
                        modified = true;
                    }
                }
            }
        }
    } else if (editMode == EditMode::Mesh || editMode == EditMode::GlobalObject) {
        if (selectedChunk.empty() || selectedObjectIdx < 0)
            return false;

        for (auto &lc : chunks) {
            if (lc.data->chunkName == selectedChunk) {
                if (selectedObjectIdx >= (int)lc.data->objects.size())
                    break;
                auto &obj = lc.data->objects[selectedObjectIdx];

                for (size_t mi = 0; mi < obj.meshes.size(); ++mi) {
                    auto &mesh = obj.meshes[mi];
                    MeshSnapshot snap;
                    if (history) {
                        snap.chunkName = selectedChunk;
                        snap.objectIdx = selectedObjectIdx;
                        snap.meshIdx = (int)mi;
                        snap.before = mesh;
                        if (obj.isGlobal) {
                            snap.hasObjectTransform = true;
                            snap.rawTxBefore = obj.rawTx;
                            snap.rawTyBefore = obj.rawTy;
                            snap.rawTzBefore = obj.rawTz;
                            memcpy(snap.rtBefore, obj.rt, sizeof(obj.rt));
                        }
                        snap.description = "Translate Object";
                    }

                    for (size_t i = 0; i < mesh.vx.size(); ++i) {
                        mesh.vx[i] += delta.x;
                        mesh.vy[i] += delta.y;
                        mesh.vz[i] += delta.z;

                        if (autoValidate) {
                            ValidationIssue issue;
                            Vector3 newPos = {mesh.vx[i], mesh.vy[i], mesh.vz[i]};
                            if (!ChunkValidator::ValidateVertexPosition(newPos, lc.data->xPos, lc.data->yPos, &issue)) {
                                printf("[AutoValidate] %s\n", issue.message.c_str());
                            }
                        }
                    }

                    if (history) {
                        snap.after = mesh;
                        if (obj.isGlobal) {
                            snap.rawTxAfter = obj.rawTx + (int32_t)std::round(delta.x * 256.0f);
                            snap.rawTyAfter = obj.rawTy - (int32_t)std::round(delta.y * 256.0f);
                            snap.rawTzAfter = obj.rawTz - (int32_t)std::round(delta.z * 256.0f);
                            memcpy(snap.rtAfter, obj.rt, sizeof(obj.rt));
                        }
                        history->Push(std::move(snap));
                    }
                }

                if (obj.bounds.min.x <= obj.bounds.max.x) {
                    obj.bounds.min.x += delta.x;
                    obj.bounds.min.y += delta.y;
                    obj.bounds.min.z += delta.z;
                    obj.bounds.max.x += delta.x;
                    obj.bounds.max.y += delta.y;
                    obj.bounds.max.z += delta.z;
                }

                if (obj.isGlobal) {
                    obj.rawTx += (int32_t)std::round(delta.x * 256.0f);
                    obj.rawTy -= (int32_t)std::round(delta.y * 256.0f);
                    obj.rawTz -= (int32_t)std::round(delta.z * 256.0f);
                }

                modified = true;
                break;
            }
        }
    }

    return modified;
}

static bool GetActiveMeshTarget(LocalGeometryOverlay& overlay, LoadedChunk*& outChunk, RenderObject*& outObj, RenderMesh*& outMesh) {
    if (overlay.m_selectedChunk.empty() || overlay.m_selectedObjectIdx < 0) return false;
    for (auto& lcConst : overlay.GetChunks()) {
        auto& lc = const_cast<LoadedChunk&>(lcConst);
        if (lc.data->chunkName == overlay.m_selectedChunk) {
            if (overlay.m_selectedObjectIdx < (int)lc.data->objects.size()) {
                auto& obj = lc.data->objects[overlay.m_selectedObjectIdx];
                if (!obj.meshes.empty()) {
                    int mIdx = (overlay.m_selectedMeshIdx >= 0 && overlay.m_selectedMeshIdx < (int)obj.meshes.size())
                               ? overlay.m_selectedMeshIdx : 0;
                    outChunk = &lc;
                    outObj = &obj;
                    outMesh = &obj.meshes[mIdx];
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

bool RotateMesh(LocalGeometryOverlay& overlay, int axis, float angleDeg, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    RenderMesh* mesh = nullptr;
    if (!GetActiveMeshTarget(overlay, lc, obj, mesh)) return false;

    // Calculate centroid of mesh
    double sumX = 0, sumY = 0, sumZ = 0;
    for (size_t i = 0; i < mesh->vx.size(); ++i) {
        sumX += mesh->vx[i]; sumY += mesh->vy[i]; sumZ += mesh->vz[i];
    }
    Vector3 center = {
        (float)(sumX / mesh->vx.size()),
        (float)(sumY / mesh->vy.size()),
        (float)(sumZ / mesh->vz.size())
    };

    float rad = angleDeg * (PI / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    MeshSnapshot snap;
    if (history) {
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0;
        snap.before = *mesh;
        snap.description = "Rotate Mesh 90°";
    }

    for (size_t i = 0; i < mesh->vx.size(); ++i) {
        float x = mesh->vx[i] - center.x;
        float y = mesh->vy[i] - center.y;
        float z = mesh->vz[i] - center.z;

        if (axis == 0) { // X-axis (Y-Z rotation)
            mesh->vy[i] = center.y + y * cosA - z * sinA;
            mesh->vz[i] = center.z + y * sinA + z * cosA;
        } else if (axis == 1) { // Y-axis (X-Z rotation)
            mesh->vx[i] = center.x + x * cosA + z * sinA;
            mesh->vz[i] = center.z - x * sinA + z * cosA;
        } else if (axis == 2) { // Z-axis (X-Y rotation)
            mesh->vx[i] = center.x + x * cosA - y * sinA;
            mesh->vy[i] = center.y + x * sinA + y * cosA;
        }
    }

    if (history) {
        snap.after = *mesh;
        history->Push(std::move(snap));
    }

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool MirrorMesh(LocalGeometryOverlay& overlay, int axis, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    RenderMesh* mesh = nullptr;
    if (!GetActiveMeshTarget(overlay, lc, obj, mesh)) return false;

    double sum = 0;
    for (size_t i = 0; i < mesh->vx.size(); ++i) {
        if (axis == 0) sum += mesh->vx[i];
        else if (axis == 1) sum += mesh->vy[i];
        else if (axis == 2) sum += mesh->vz[i];
    }
    float centerAxis = (float)(sum / mesh->vx.size());

    MeshSnapshot snap;
    if (history) {
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0;
        snap.before = *mesh;
        snap.description = "Mirror Mesh";
    }

    for (size_t i = 0; i < mesh->vx.size(); ++i) {
        if (axis == 0) mesh->vx[i] = 2.0f * centerAxis - mesh->vx[i];
        else if (axis == 1) mesh->vy[i] = 2.0f * centerAxis - mesh->vy[i];
        else if (axis == 2) mesh->vz[i] = 2.0f * centerAxis - mesh->vz[i];
    }

    // Invert face winding
    for (auto& face : mesh->faces) {
        InvertPolygonWinding(face.v, face.uv, face.rawU, face.rawV);
    }

    if (history) {
        snap.after = *mesh;
        history->Push(std::move(snap));
    }

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool SnapMeshToFloor(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    RenderMesh* mesh = nullptr;
    if (!GetActiveMeshTarget(overlay, lc, obj, mesh)) return false;

    float minY = 99999.0f;
    for (float y : mesh->vy) {
        if (y < minY) minY = y;
    }
    if (minY > 90000.0f) return false;

    double sumX = 0, sumZ = 0;
    for (size_t i = 0; i < mesh->vx.size(); ++i) {
        sumX += mesh->vx[i];
        sumZ += mesh->vz[i];
    }
    float centerX = (float)(sumX / mesh->vx.size());
    float centerZ = (float)(sumZ / mesh->vz.size());

    float targetFloorY = FindFloorHeightBelow(overlay, centerX, centerZ, minY, lc->data->chunkName, overlay.m_selectedObjectIdx);
    if (targetFloorY < -90000.0f) {
        targetFloorY = 0.0f;
    }

    float deltaY = targetFloorY - minY;
    if (fabsf(deltaY) < 0.0001f) return false;

    MeshSnapshot snap;
    if (history) {
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0;
        snap.before = *mesh;
        snap.description = "Snap Mesh to Floor";
    }

    for (float& y : mesh->vy) {
        y += deltaY;
    }

    if (history) {
        snap.after = *mesh;
        history->Push(std::move(snap));
    }

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool DuplicateMesh(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    RenderMesh* mesh = nullptr;
    if (!GetActiveMeshTarget(overlay, lc, obj, mesh)) return false;

    RenderMesh newMesh = *mesh;
    for (float& x : newMesh.vx) x += 0.5f;

    obj->meshes.push_back(std::move(newMesh));
    overlay.m_selectedMeshIdx = (int)obj->meshes.size() - 1;

    if (history) {
        MeshSnapshot snap;
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = overlay.m_selectedMeshIdx;
        snap.after = obj->meshes.back();
        snap.description = "Duplicate Mesh";
        history->Push(std::move(snap));
    }

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool SeparateMeshToNewObject(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    RenderMesh* mesh = nullptr;
    if (!GetActiveMeshTarget(overlay, lc, obj, mesh)) return false;
    if (obj->meshes.size() <= 1) return false;

    int mIdx = overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0;
    RenderObject newObj;
    newObj.name = obj->name + "_S";
    newObj.isGlobal = false;
    newObj.ipdDataOffset = -1;
    newObj.meshes.push_back(std::move(obj->meshes[mIdx]));
    obj->meshes.erase(obj->meshes.begin() + mIdx);

    lc->data->objects.push_back(std::move(newObj));
    overlay.m_selectedObjectIdx = (int)lc->data->objects.size() - 1;
    overlay.m_selectedMeshIdx = 0;

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool MergeMeshes(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    RenderMesh* mesh = nullptr;
    if (!GetActiveMeshTarget(overlay, lc, obj, mesh)) return false;
    if (obj->meshes.size() <= 1) return false;

    size_t totalVerts = 0;
    for (const auto& m : obj->meshes) totalVerts += m.vx.size();
    if (totalVerts > VALIDATOR_MAX_VERTS) {
        printf("[MergeMeshes] Cannot merge: combined vertex count (%zu) exceeds PS1 limit of %d\n",
               totalVerts, VALIDATOR_MAX_VERTS);
        return false;
    }

    RenderMesh combined = obj->meshes[0];
    for (size_t mi = 1; mi < obj->meshes.size(); ++mi) {
        const auto& src = obj->meshes[mi];
        uint8_t baseVert = (uint8_t)combined.vx.size();

        combined.vx.insert(combined.vx.end(), src.vx.begin(), src.vx.end());
        combined.vy.insert(combined.vy.end(), src.vy.begin(), src.vy.end());
        combined.vz.insert(combined.vz.end(), src.vz.begin(), src.vz.end());

        for (auto f : src.faces) {
            f.v[0] += baseVert;
            f.v[1] += baseVert;
            f.v[2] += baseVert;
            if (f.v[3] != 0xFF) f.v[3] += baseVert;
            combined.faces.push_back(f);
        }
    }

    obj->meshes.clear();
    obj->meshes.push_back(std::move(combined));
    overlay.m_selectedMeshIdx = 0;

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool DeleteMesh(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    RenderMesh* mesh = nullptr;
    if (!GetActiveMeshTarget(overlay, lc, obj, mesh)) return false;

    int mIdx = overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0;
    obj->meshes.erase(obj->meshes.begin() + mIdx);
    if (obj->meshes.empty()) {
        lc->data->objects.erase(lc->data->objects.begin() + overlay.m_selectedObjectIdx);
        overlay.m_selectedObjectIdx = -1;
    }
    overlay.m_selectedMeshIdx = -1;

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool RecalculateBounds(LocalGeometryOverlay& overlay) {
    if (overlay.m_selectedChunk.empty()) return false;
    for (auto& lcConst : overlay.GetChunks()) {
        auto& lc = const_cast<LoadedChunk&>(lcConst);
        if (lc.data->chunkName == overlay.m_selectedChunk) {
            for (auto& obj : lc.data->objects) {
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
            return true;
        }
    }
    return false;
}

bool CenterMeshPivot(LocalGeometryOverlay& overlay, History* history) {
    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    RenderMesh* mesh = nullptr;
    if (!GetActiveMeshTarget(overlay, lc, obj, mesh)) return false;

    double sumX = 0, sumY = 0, sumZ = 0;
    for (size_t i = 0; i < mesh->vx.size(); ++i) {
        sumX += mesh->vx[i]; sumY += mesh->vy[i]; sumZ += mesh->vz[i];
    }
    Vector3 center = {
        (float)(sumX / mesh->vx.size()),
        (float)(sumY / mesh->vy.size()),
        (float)(sumZ / mesh->vz.size())
    };

    for (size_t i = 0; i < mesh->vx.size(); ++i) {
        mesh->vx[i] -= center.x;
        mesh->vy[i] -= center.y;
        mesh->vz[i] -= center.z;
    }

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
}

bool AddPrimitive(LocalGeometryOverlay& overlay, int primType, float width, float height, float length, History* history) {
    if (overlay.m_selectedChunk.empty()) return false;

    LoadedChunk* targetLc = nullptr;
    for (auto& lcConst : overlay.GetChunks()) {
        auto& lc = const_cast<LoadedChunk&>(lcConst);
        if (lc.data->chunkName == overlay.m_selectedChunk) {
            targetLc = &lc;
            break;
        }
    }
    if (!targetLc) return false;

    Vector3 center = ChunkOwnership::GridToWorldCenter(targetLc->data->xPos, targetLc->data->yPos);
    float baseX = center.x;
    float baseY = 0.0f;
    float baseZ = center.z;

    float hw = width * 0.5f;
    float hh = height;
    float hl = length * 0.5f;

    RenderMesh newMesh;

    auto makeFace = [](uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3 = 0xFF) {
        RenderFace f;
        memset(&f, 0, sizeof(f));
        f.v[0] = v0; f.v[1] = v1; f.v[2] = v2; f.v[3] = v3;
        f.texNum = 0x7F; // untextured
        f.uv[0][0] = 0.0f; f.uv[0][1] = 0.0f;
        f.uv[1][0] = 1.0f; f.uv[1][1] = 0.0f;
        f.uv[2][0] = 1.0f; f.uv[2][1] = 1.0f;
        f.uv[3][0] = 0.0f; f.uv[3][1] = 1.0f;
        return f;
    };

    if (primType == 0) { // Floor Plane (XZ)
        newMesh.vx = {baseX - hw, baseX + hw, baseX + hw, baseX - hw};
        newMesh.vy = {baseY, baseY, baseY, baseY};
        newMesh.vz = {baseZ - hl, baseZ - hl, baseZ + hl, baseZ + hl};
        newMesh.faces.push_back(makeFace(0, 1, 2, 3));
    } else if (primType == 1) { // Wall Plane (XY)
        newMesh.vx = {baseX - hw, baseX + hw, baseX + hw, baseX - hw};
        newMesh.vy = {baseY, baseY, baseY + hh, baseY + hh};
        newMesh.vz = {baseZ, baseZ, baseZ, baseZ};
        newMesh.faces.push_back(makeFace(0, 1, 2, 3));
    } else if (primType == 2) { // Cube Block (8 verts, 6 quads)
        newMesh.vx = {baseX - hw, baseX + hw, baseX + hw, baseX - hw,  baseX - hw, baseX + hw, baseX + hw, baseX - hw};
        newMesh.vy = {baseY, baseY, baseY, baseY,                      baseY + hh, baseY + hh, baseY + hh, baseY + hh};
        newMesh.vz = {baseZ - hl, baseZ - hl, baseZ + hl, baseZ + hl,  baseZ - hl, baseZ - hl, baseZ + hl, baseZ + hl};

        newMesh.faces.push_back(makeFace(0, 3, 2, 1)); // Bottom
        newMesh.faces.push_back(makeFace(4, 5, 6, 7)); // Top
        newMesh.faces.push_back(makeFace(0, 1, 5, 4)); // Front
        newMesh.faces.push_back(makeFace(2, 3, 7, 6)); // Back
        newMesh.faces.push_back(makeFace(3, 0, 4, 7)); // Left
        newMesh.faces.push_back(makeFace(1, 2, 6, 5)); // Right
    } else if (primType == 3) { // Ramp / Slope
        newMesh.vx = {baseX - hw, baseX + hw, baseX + hw, baseX - hw,  baseX - hw, baseX + hw};
        newMesh.vy = {baseY, baseY, baseY, baseY,                      baseY + hh, baseY + hh};
        newMesh.vz = {baseZ - hl, baseZ - hl, baseZ + hl, baseZ + hl,  baseZ + hl, baseZ + hl};

        newMesh.faces.push_back(makeFace(0, 3, 2, 1)); // Bottom
        newMesh.faces.push_back(makeFace(0, 1, 5, 4)); // Ramp Slope
        newMesh.faces.push_back(makeFace(2, 3, 4, 5)); // Back Wall
        newMesh.faces.push_back(makeFace(3, 0, 4));    // Left Tri
        newMesh.faces.push_back(makeFace(1, 2, 5));    // Right Tri
    }

    RenderObject newObj;
    newObj.name = "PRIM_OBJ";
    newObj.isGlobal = false;
    newObj.ipdDataOffset = -1;
    newObj.meshes.push_back(std::move(newMesh));

    targetLc->data->objects.push_back(std::move(newObj));
    overlay.m_selectedObjectIdx = (int)targetLc->data->objects.size() - 1;
    overlay.m_selectedMeshIdx = 0;

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(targetLc->data->chunkName, ws);
    return true;
}

} // namespace Geometry
