#include "geometry/VertexOperations.h"
#include "geometry/MeshOperations.h"
#include "geometry/TransformOperations.h"
#include "geometry/GeometryCommon.h"
#include "viewport/LocalGeometryOverlay.h"
#include "core/History.h"
#include "formats/IPDWrite.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace Geometry {

static std::set<SelectedVertex> GetEffectiveSelectedVertices(const LocalGeometryOverlay& overlay) {
    std::set<SelectedVertex> result = overlay.m_selectedVertices;
    if (result.empty() && overlay.m_selectedVertexIdx >= 0 && !overlay.m_selectedChunk.empty() && overlay.m_selectedObjectIdx >= 0) {
        result.insert({overlay.m_selectedChunk, overlay.m_selectedObjectIdx, overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0, overlay.m_selectedVertexIdx});
    }
    return result;
}

// ---------------------------------------------------------------------------
// SnapVerticesToGrid
// ---------------------------------------------------------------------------
bool SnapVerticesToGrid(LocalGeometryOverlay& overlay, History* history) {
    auto selVerts = GetEffectiveSelectedVertices(overlay);
    if (selVerts.empty()) return false;

    int rawUnits = 1 << overlay.m_moveStepPower;
    float step = (float)rawUnits / 256.0f;
    if (step <= 0.0001f) step = 1.0f / 256.0f;

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

            std::set<int> vertIndices;
            for (const auto& sv : selVerts) {
                if (sv.chunkName == cName && sv.objectIdx == oIdx && sv.meshIdx == mIdx)
                    vertIndices.insert(sv.vertexIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Snap Vertices to Grid";
            }

            bool changed = false;
            for (int vIdx : vertIndices) {
                if (vIdx < 0 || vIdx >= (int)mesh.vx.size()) continue;
                mesh.vx[vIdx] = std::round(mesh.vx[vIdx] / step) * step;
                mesh.vy[vIdx] = std::round(mesh.vy[vIdx] / step) * step;
                mesh.vz[vIdx] = std::round(mesh.vz[vIdx] / step) * step;
                changed = true;
            }

            if (changed) {
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

// ---------------------------------------------------------------------------
// SnapVerticesToFloor
// ---------------------------------------------------------------------------
bool SnapVerticesToFloor(LocalGeometryOverlay& overlay, History* history) {
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

            std::set<int> vertIndices;
            for (const auto& sv : selVerts) {
                if (sv.chunkName == cName && sv.objectIdx == oIdx && sv.meshIdx == mIdx)
                    vertIndices.insert(sv.vertexIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Snap Vertices to Floor";
            }

            bool changed = false;
            for (int vIdx : vertIndices) {
                if (vIdx < 0 || vIdx >= (int)mesh.vx.size()) continue;
                float flrY = FindFloorHeightBelow(overlay, mesh.vx[vIdx], mesh.vz[vIdx], mesh.vy[vIdx], cName, oIdx);
                if (flrY > -90000.0f) {
                    mesh.vy[vIdx] = flrY;
                } else {
                    mesh.vy[vIdx] = 0.0f;
                }
                changed = true;
            }

            if (changed) {
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

// ---------------------------------------------------------------------------
// PlanarizeVertices
// ---------------------------------------------------------------------------
bool PlanarizeVertices(LocalGeometryOverlay& overlay, int axis, History* history) {
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

            std::set<int> vertIndices;
            for (const auto& sv : selVerts) {
                if (sv.chunkName == cName && sv.objectIdx == oIdx && sv.meshIdx == mIdx)
                    vertIndices.insert(sv.vertexIdx);
            }

            if (vertIndices.empty()) continue;

            double sum = 0.0;
            int count = 0;
            for (int vIdx : vertIndices) {
                if (vIdx < 0 || vIdx >= (int)mesh.vx.size()) continue;
                if (axis == 0) sum += mesh.vx[vIdx];
                else if (axis == 1) sum += mesh.vy[vIdx];
                else if (axis == 2) sum += mesh.vz[vIdx];
                count++;
            }

            if (count == 0) continue;
            float avg = (float)(sum / count);

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = (axis == 0) ? "Flatten X" : (axis == 1 ? "Flatten Y" : "Flatten Z");
            }

            for (int vIdx : vertIndices) {
                if (vIdx < 0 || vIdx >= (int)mesh.vx.size()) continue;
                if (axis == 0) mesh.vx[vIdx] = avg;
                else if (axis == 1) mesh.vy[vIdx] = avg;
                else if (axis == 2) mesh.vz[vIdx] = avg;
            }

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
    return anyModified;
}

// ---------------------------------------------------------------------------
// AddFaceFromSelectedVertices
// ---------------------------------------------------------------------------
bool AddFaceFromSelectedVertices(LocalGeometryOverlay& overlay, History* history) {
    auto selVerts = GetEffectiveSelectedVertices(overlay);
    if (selVerts.size() != 3 && selVerts.size() != 4) {
        printf("[Geometry] Add Face: Must select exactly 3 or 4 vertices (selected %zu)\n", selVerts.size());
        return false;
    }

    std::string cName;
    int oIdx = -1, mIdx = -1;
    std::vector<int> vertIndices;

    for (const auto& sv : selVerts) {
        if (cName.empty()) {
            cName = sv.chunkName; oIdx = sv.objectIdx; mIdx = sv.meshIdx;
        }
        if (sv.chunkName == cName && sv.objectIdx == oIdx && sv.meshIdx == mIdx) {
            vertIndices.push_back(sv.vertexIdx);
        }
    }

    if (vertIndices.size() != 3 && vertIndices.size() != 4) return false;

    for (const auto& lcConst : overlay.GetChunks()) {
        auto& lc = const_cast<LoadedChunk&>(lcConst);
        if (lc.data->chunkName != cName || oIdx >= (int)lc.data->objects.size()) continue;
        auto& obj = lc.data->objects[oIdx];
        if (mIdx >= (int)obj.meshes.size()) break;
        auto& mesh = obj.meshes[mIdx];

        MeshSnapshot snap;
        if (history) {
            snap.chunkName = cName;
            snap.objectIdx = oIdx;
            snap.meshIdx = mIdx;
            snap.before = mesh;
            snap.description = "Add Face from Vertices";
        }

        RenderFace newFace;
        memset(&newFace, 0, sizeof(newFace));
        newFace.addr.plmObjectName = obj.name;
        newFace.addr.meshIdx = mIdx;
        newFace.addr.isGlobal = obj.isGlobal;

        // Inherit default texture properties from first existing face if available
        if (!mesh.faces.empty()) {
            newFace.texName = mesh.faces[0].texName;
            newFace.texNum = mesh.faces[0].texNum;
            newFace.paletteRow = mesh.faces[0].paletteRow;
            newFace.cbaRaw = mesh.faces[0].cbaRaw;
        } else {
            newFace.texNum = 0x7F;
        }

        if (vertIndices.size() == 3) {
            newFace.v[0] = (uint8_t)vertIndices[0];
            newFace.v[1] = (uint8_t)vertIndices[1];
            newFace.v[2] = (uint8_t)vertIndices[2];
            newFace.v[3] = 0xFF;

            newFace.uv[0][0] = 0.0f; newFace.uv[0][1] = 0.0f;
            newFace.uv[1][0] = 1.0f; newFace.uv[1][1] = 0.0f;
            newFace.uv[2][0] = 1.0f; newFace.uv[2][1] = 1.0f;

            newFace.rawU[0] = 0;   newFace.rawU[1] = 255; newFace.rawU[2] = 255;
            newFace.rawV[0] = 0;   newFace.rawV[1] = 0;   newFace.rawV[2] = 255;
        } else {
            // Sort 4 vertices around their centroid in the best-fit plane to prevent self-intersection
            Vector3 center = { 0, 0, 0 };
            for (int vi : vertIndices) {
                center.x += mesh.vx[vi]; center.y += mesh.vy[vi]; center.z += mesh.vz[vi];
            }
            center.x /= 4.0f; center.y /= 4.0f; center.z /= 4.0f;

            Vector3 v0 = { mesh.vx[vertIndices[0]], mesh.vy[vertIndices[0]], mesh.vz[vertIndices[0]] };
            Vector3 v1 = { mesh.vx[vertIndices[1]], mesh.vy[vertIndices[1]], mesh.vz[vertIndices[1]] };
            Vector3 v2 = { mesh.vx[vertIndices[2]], mesh.vy[vertIndices[2]], mesh.vz[vertIndices[2]] };
            Vector3 norm = ComputeTriangleNormal(v0, v1, v2);

            Vector3 axisU = Vector3Normalize(Vector3Subtract(v0, center));
            if (Vector3Length(axisU) < 0.001f) axisU = { 1.0f, 0.0f, 0.0f };
            Vector3 axisV = Vector3CrossProduct(norm, axisU);

            struct AngleSort {
                int vIdx;
                float angle;
            };
            std::vector<AngleSort> sorted;
            for (int vi : vertIndices) {
                Vector3 toV = { mesh.vx[vi] - center.x, mesh.vy[vi] - center.y, mesh.vz[vi] - center.z };
                float u = Vector3DotProduct(toV, axisU);
                float v = Vector3DotProduct(toV, axisV);
                float ang = std::atan2(v, u);
                sorted.push_back({ vi, ang });
            }
            std::sort(sorted.begin(), sorted.end(), [](const AngleSort& a, const AngleSort& b) {
                return a.angle < b.angle;
            });

            newFace.v[0] = (uint8_t)sorted[0].vIdx;
            newFace.v[1] = (uint8_t)sorted[1].vIdx;
            newFace.v[2] = (uint8_t)sorted[2].vIdx;
            newFace.v[3] = (uint8_t)sorted[3].vIdx;

            newFace.uv[0][0] = 0.0f; newFace.uv[0][1] = 0.0f;
            newFace.uv[1][0] = 1.0f; newFace.uv[1][1] = 0.0f;
            newFace.uv[2][0] = 1.0f; newFace.uv[2][1] = 1.0f;
            newFace.uv[3][0] = 0.0f; newFace.uv[3][1] = 1.0f;

            newFace.rawU[0] = 0;   newFace.rawU[1] = 255; newFace.rawU[2] = 255; newFace.rawU[3] = 0;
            newFace.rawV[0] = 0;   newFace.rawV[1] = 0;   newFace.rawV[2] = 255; newFace.rawV[3] = 255;
        }

        mesh.faces.push_back(newFace);

        if (history) {
            snap.after = mesh;
            history->Push(std::move(snap));
        }

        RecalculateBounds(overlay);
        std::string ws = GetWorkspaceDir(overlay);
        overlay.RebuildChunkBatches(cName, ws);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ExtrudeSelectedVertices
// ---------------------------------------------------------------------------
bool ExtrudeSelectedVertices(LocalGeometryOverlay& overlay, Vector3 offset, History* history) {
    auto selVerts = GetEffectiveSelectedVertices(overlay);
    if (selVerts.empty()) return false;

    float step = Vector3Length(offset);
    if (step <= 0.0001f) {
        int rawUnits = 1 << overlay.m_moveStepPower;
        step = (float)rawUnits / 256.0f;
        offset = { 0.0f, step, 0.0f };
    }

    std::set<std::tuple<std::string, int, int>> modifiedMeshes;
    for (const auto& sv : selVerts) {
        modifiedMeshes.insert({sv.chunkName, sv.objectIdx, sv.meshIdx});
    }

    bool anyModified = false;
    std::set<SelectedVertex> newGlobalSelectedVerts;
    int newPrimarySelectedVertex = -1;
    std::string lastChunkName;
    int lastObjIdx = -1, lastMeshIdx = -1;

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
            if (mesh.vx.size() + vertIndices.size() > 255) {
                printf("[Geometry] Vertex capacity (255) exceeded on mesh '%s'\n", obj.name.c_str());
                continue;
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Extrude Vertices";
            }

            int foundFaceIdx = -1;
            if (vertIndices.size() == 4) {
                std::set<uint8_t> selSet = {
                    (uint8_t)vertIndices[0],
                    (uint8_t)vertIndices[1],
                    (uint8_t)vertIndices[2],
                    (uint8_t)vertIndices[3]
                };
                for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                    const auto& f = mesh.faces[fi];
                    if (f.v[3] != 0xFF) {
                        std::set<uint8_t> fSet = { f.v[0], f.v[1], f.v[2], f.v[3] };
                        if (fSet == selSet) {
                            foundFaceIdx = (int)fi;
                            break;
                        }
                    }
                }
            }

            if (vertIndices.size() == 4 && foundFaceIdx >= 0) {
                // 1. 4 vertices contain an existing quad face:
                // Extrude face along normal, creating 5 faces (4 outward side quads + 1 cap quad),
                // remove original face, and select the new cap face vertices.
                const RenderFace origFace = mesh.faces[foundFaceIdx];
                uint8_t v0 = origFace.v[0];
                uint8_t v1 = origFace.v[1];
                uint8_t v2 = origFace.v[2];
                uint8_t v3 = origFace.v[3];
                uint8_t origV[4] = { v0, v1, v2, v3 };

                Vector3 p0 = { mesh.vx[v0], mesh.vy[v0], mesh.vz[v0] };
                Vector3 p1 = { mesh.vx[v1], mesh.vy[v1], mesh.vz[v1] };
                Vector3 p2 = { mesh.vx[v2], mesh.vy[v2], mesh.vz[v2] };
                Vector3 p3 = { mesh.vx[v3], mesh.vy[v3], mesh.vz[v3] };

                Vector3 norm = ComputeTriangleNormal(p0, p1, p2);
                Vector3 finalOffset = Vector3Scale(norm, step);

                uint8_t nv0 = (uint8_t)mesh.vx.size();
                mesh.vx.push_back(p0.x + finalOffset.x);
                mesh.vy.push_back(p0.y + finalOffset.y);
                mesh.vz.push_back(p0.z + finalOffset.z);

                uint8_t nv1 = (uint8_t)mesh.vx.size();
                mesh.vx.push_back(p1.x + finalOffset.x);
                mesh.vy.push_back(p1.y + finalOffset.y);
                mesh.vz.push_back(p1.z + finalOffset.z);

                uint8_t nv2 = (uint8_t)mesh.vx.size();
                mesh.vx.push_back(p2.x + finalOffset.x);
                mesh.vy.push_back(p2.y + finalOffset.y);
                mesh.vz.push_back(p2.z + finalOffset.z);

                uint8_t nv3 = (uint8_t)mesh.vx.size();
                mesh.vx.push_back(p3.x + finalOffset.x);
                mesh.vy.push_back(p3.y + finalOffset.y);
                mesh.vz.push_back(p3.z + finalOffset.z);

                uint8_t newV[4] = { nv0, nv1, nv2, nv3 };

                bool hasTexture = (origFace.texNum != 0x7F && !origFace.texName.empty());
                float minU = 0.0f, maxU = 1.0f, minV = 0.0f, maxV = 1.0f;
                uint8_t minRawU = 0, maxRawU = 255, minRawV = 0, maxRawV = 255;

                if (hasTexture) {
                    minU = std::min({origFace.uv[0][0], origFace.uv[1][0], origFace.uv[2][0], origFace.uv[3][0]});
                    maxU = std::max({origFace.uv[0][0], origFace.uv[1][0], origFace.uv[2][0], origFace.uv[3][0]});
                    minV = std::min({origFace.uv[0][1], origFace.uv[1][1], origFace.uv[2][1], origFace.uv[3][1]});
                    maxV = std::max({origFace.uv[0][1], origFace.uv[1][1], origFace.uv[2][1], origFace.uv[3][1]});

                    minRawU = std::min({origFace.rawU[0], origFace.rawU[1], origFace.rawU[2], origFace.rawU[3]});
                    maxRawU = std::max({origFace.rawU[0], origFace.rawU[1], origFace.rawU[2], origFace.rawU[3]});
                    minRawV = std::min({origFace.rawV[0], origFace.rawV[1], origFace.rawV[2], origFace.rawV[3]});
                    maxRawV = std::max({origFace.rawV[0], origFace.rawV[1], origFace.rawV[2], origFace.rawV[3]});
                }

                std::vector<RenderFace> newFaces;

                // 4 side faces (outward facing)
                for (int i = 0; i < 4; ++i) {
                    int nextI = (i + 1) % 4;
                    RenderFace side = origFace;
                    side.v[0] = origV[i];
                    side.v[1] = origV[nextI];
                    side.v[2] = newV[nextI];
                    side.v[3] = newV[i];

                    if (hasTexture) {
                        side.uv[0][0] = minU; side.uv[0][1] = minV;
                        side.uv[1][0] = maxU; side.uv[1][1] = minV;
                        side.uv[2][0] = maxU; side.uv[2][1] = maxV;
                        side.uv[3][0] = minU; side.uv[3][1] = maxV;

                        side.rawU[0] = minRawU; side.rawU[1] = maxRawU; side.rawU[2] = maxRawU; side.rawU[3] = minRawU;
                        side.rawV[0] = minRawV; side.rawV[1] = minRawV; side.rawV[2] = maxRawV; side.rawV[3] = maxRawV;
                    } else {
                        side.texNum = 0x7F;
                        side.texName = "";
                        side.paletteRow = 0;
                        side.cbaRaw = 0;
                        for (int k = 0; k < 4; ++k) {
                            side.uv[k][0] = 0.0f; side.uv[k][1] = 0.0f;
                            side.rawU[k] = 0;     side.rawV[k] = 0;
                        }
                    }
                    newFaces.push_back(side);
                }

                // 1 cap face across from the original face, preserving rotation/mirroring
                RenderFace capFace = origFace;
                capFace.v[0] = nv0;
                capFace.v[1] = nv1;
                capFace.v[2] = nv2;
                capFace.v[3] = nv3;
                newFaces.push_back(capFace);

                // Remove the original face
                mesh.faces.erase(mesh.faces.begin() + foundFaceIdx);

                // Add the 5 new faces
                for (const auto& nf : newFaces) {
                    mesh.faces.push_back(nf);
                }

                newGlobalSelectedVerts.insert({ cName, oIdx, mIdx, nv0 });
                newGlobalSelectedVerts.insert({ cName, oIdx, mIdx, nv1 });
                newGlobalSelectedVerts.insert({ cName, oIdx, mIdx, nv2 });
                newGlobalSelectedVerts.insert({ cName, oIdx, mIdx, nv3 });
                if (newPrimarySelectedVertex < 0) newPrimarySelectedVertex = nv0;
                lastChunkName = cName; lastObjIdx = oIdx; lastMeshIdx = mIdx;
            } else if (vertIndices.size() == 4) {
                // 2. 4 vertices do NOT contain a face:
                // Only duplicate the vertices, do not create any faces.
                for (int vi : vertIndices) {
                    uint8_t newIdx = (uint8_t)mesh.vx.size();
                    mesh.vx.push_back(mesh.vx[vi] + offset.x);
                    mesh.vy.push_back(mesh.vy[vi] + offset.y);
                    mesh.vz.push_back(mesh.vz[vi] + offset.z);
                    newGlobalSelectedVerts.insert({ cName, oIdx, mIdx, newIdx });
                    if (newPrimarySelectedVertex < 0) newPrimarySelectedVertex = newIdx;
                }
                lastChunkName = cName; lastObjIdx = oIdx; lastMeshIdx = mIdx;
            } else {
                // 3. Other count of vertices (e.g. 2 vertices for edge extrusion):
                std::map<uint8_t, uint8_t> oldToNew;
                for (int vi : vertIndices) {
                    uint8_t newIdx = (uint8_t)mesh.vx.size();
                    mesh.vx.push_back(mesh.vx[vi] + offset.x);
                    mesh.vy.push_back(mesh.vy[vi] + offset.y);
                    mesh.vz.push_back(mesh.vz[vi] + offset.z);
                    oldToNew[(uint8_t)vi] = newIdx;
                    newGlobalSelectedVerts.insert({ cName, oIdx, mIdx, newIdx });
                    if (newPrimarySelectedVertex < 0) newPrimarySelectedVertex = newIdx;
                }
                lastChunkName = cName; lastObjIdx = oIdx; lastMeshIdx = mIdx;

                if (vertIndices.size() == 2) {
                    std::set<std::pair<uint8_t, uint8_t>> existingEdges;
                    std::vector<RenderFace> bridgeFaces;
                    for (const auto& f : mesh.faces) {
                        bool isQuad = (f.v[3] != 0xFF);
                        int numV = isQuad ? 4 : 3;
                        for (int i = 0; i < numV; ++i) {
                            uint8_t a = f.v[i];
                            uint8_t b = f.v[(i + 1) % numV];
                            if (oldToNew.find(a) != oldToNew.end() && oldToNew.find(b) != oldToNew.end()) {
                                uint8_t minV = std::min(a, b);
                                uint8_t maxV = std::max(a, b);
                                if (existingEdges.find({minV, maxV}) == existingEdges.end()) {
                                    existingEdges.insert({minV, maxV});

                                    RenderFace bridge = f;
                                    bridge.v[0] = a;
                                    bridge.v[1] = b;
                                    bridge.v[2] = oldToNew[b];
                                    bridge.v[3] = oldToNew[a];

                                    bridge.uv[0][0] = 0.0f; bridge.uv[0][1] = 0.0f;
                                    bridge.uv[1][0] = 1.0f; bridge.uv[1][1] = 0.0f;
                                    bridge.uv[2][0] = 1.0f; bridge.uv[2][1] = 1.0f;
                                    bridge.uv[3][0] = 0.0f; bridge.uv[3][1] = 1.0f;

                                    bridge.rawU[0] = 0; bridge.rawU[1] = 255; bridge.rawU[2] = 255; bridge.rawU[3] = 0;
                                    bridge.rawV[0] = 0; bridge.rawV[1] = 0;   bridge.rawV[2] = 255; bridge.rawV[3] = 255;

                                    bridgeFaces.push_back(bridge);
                                }
                            }
                        }
                    }
                    for (const auto& bf : bridgeFaces) {
                        mesh.faces.push_back(bf);
                    }
                }
            }

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

    if (anyModified && !newGlobalSelectedVerts.empty()) {
        auto& mutableOverlay = const_cast<LocalGeometryOverlay&>(overlay);
        mutableOverlay.m_selectedVertices = newGlobalSelectedVerts;
        mutableOverlay.m_selectedVertexIdx = newPrimarySelectedVertex;
        if (!lastChunkName.empty()) {
            mutableOverlay.m_selectedChunk = lastChunkName;
            mutableOverlay.m_selectedObjectIdx = lastObjIdx;
            mutableOverlay.m_selectedMeshIdx = lastMeshIdx;
        }
    }

    return anyModified;
}

// ---------------------------------------------------------------------------
// WeldVertices
// ---------------------------------------------------------------------------
bool WeldVertices(LocalGeometryOverlay& overlay, float tolerance, History* history) {
    if (tolerance <= 0.0001f) tolerance = 0.05f;

    auto selVerts = GetEffectiveSelectedVertices(overlay);

    LoadedChunk* targetLc = nullptr;
    RenderObject* targetObj = nullptr;
    RenderMesh* targetMesh = nullptr;

    if (!overlay.m_selectedChunk.empty() && overlay.m_selectedObjectIdx >= 0) {
        for (const auto& lcConst : overlay.GetChunks()) {
            auto& lc = const_cast<LoadedChunk&>(lcConst);
            if (lc.data->chunkName == overlay.m_selectedChunk) {
                if (overlay.m_selectedObjectIdx < (int)lc.data->objects.size()) {
                    targetLc = &lc;
                    targetObj = &lc.data->objects[overlay.m_selectedObjectIdx];
                    int mIdx = overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0;
                    if (mIdx < (int)targetObj->meshes.size()) {
                        targetMesh = &targetObj->meshes[mIdx];
                    }
                }
                break;
            }
        }
    }

    if (!targetMesh || !targetLc) return false;

    MeshSnapshot snap;
    if (history) {
        snap.chunkName = targetLc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0;
        snap.before = *targetMesh;
        snap.description = "Weld Vertices";
    }

    size_t numV = targetMesh->vx.size();
    std::vector<int> remap(numV);
    for (size_t i = 0; i < numV; ++i) remap[i] = (int)i;

    bool weldedAny = false;

    // Cluster coincident vertices within tolerance
    for (size_t i = 0; i < numV; ++i) {
        if (remap[i] != (int)i) continue;
        Vector3 vi = { targetMesh->vx[i], targetMesh->vy[i], targetMesh->vz[i] };
        for (size_t j = i + 1; j < numV; ++j) {
            if (remap[j] != (int)j) continue;
            Vector3 vj = { targetMesh->vx[j], targetMesh->vy[j], targetMesh->vz[j] };
            if (Vector3Distance(vi, vj) <= tolerance) {
                remap[j] = (int)i;
                weldedAny = true;
            }
        }
    }

    if (!weldedAny) {
        printf("[Geometry] Weld: No vertices within %.3fm tolerance found.\n", tolerance);
        return false;
    }

    // Remap all faces and remove collapsed/degenerate faces
    std::vector<RenderFace> newFaces;
    for (auto& f : targetMesh->faces) {
        uint8_t v0 = (uint8_t)remap[f.v[0]];
        uint8_t v1 = (uint8_t)remap[f.v[1]];
        uint8_t v2 = (uint8_t)remap[f.v[2]];
        uint8_t v3 = (f.v[3] != 0xFF) ? (uint8_t)remap[f.v[3]] : 0xFF;

        if (v3 != 0xFF) {
            // Quad face
            std::set<uint8_t> uniqueV = { v0, v1, v2, v3 };
            if (uniqueV.size() == 4) {
                f.v[0] = v0; f.v[1] = v1; f.v[2] = v2; f.v[3] = v3;
                newFaces.push_back(f);
            } else if (uniqueV.size() == 3) {
                // Collapsed into triangle
                RenderFace tri = f;
                if (v0 == v1) { tri.v[0] = v1; tri.v[1] = v2; tri.v[2] = v3; }
                else if (v1 == v2) { tri.v[0] = v0; tri.v[1] = v2; tri.v[2] = v3; }
                else if (v2 == v3) { tri.v[0] = v0; tri.v[1] = v1; tri.v[2] = v3; }
                else { tri.v[0] = v0; tri.v[1] = v1; tri.v[2] = v2; }
                tri.v[3] = 0xFF;
                newFaces.push_back(tri);
            }
        } else {
            // Triangle face
            std::set<uint8_t> uniqueV = { v0, v1, v2 };
            if (uniqueV.size() == 3) {
                f.v[0] = v0; f.v[1] = v1; f.v[2] = v2;
                newFaces.push_back(f);
            }
        }
    }
    targetMesh->faces = std::move(newFaces);

    // Compact unused vertices
    std::vector<bool> vertUsed(numV, false);
    for (const auto& f : targetMesh->faces) {
        vertUsed[f.v[0]] = true;
        vertUsed[f.v[1]] = true;
        vertUsed[f.v[2]] = true;
        if (f.v[3] != 0xFF) vertUsed[f.v[3]] = true;
    }

    std::vector<float> newVx, newVy, newVz;
    std::vector<int> compactRemap(numV, -1);
    for (size_t i = 0; i < numV; ++i) {
        if (vertUsed[i]) {
            compactRemap[i] = (int)newVx.size();
            newVx.push_back(targetMesh->vx[i]);
            newVy.push_back(targetMesh->vy[i]);
            newVz.push_back(targetMesh->vz[i]);
        }
    }

    targetMesh->vx = std::move(newVx);
    targetMesh->vy = std::move(newVy);
    targetMesh->vz = std::move(newVz);

    for (auto& f : targetMesh->faces) {
        f.v[0] = (uint8_t)compactRemap[f.v[0]];
        f.v[1] = (uint8_t)compactRemap[f.v[1]];
        f.v[2] = (uint8_t)compactRemap[f.v[2]];
        if (f.v[3] != 0xFF) f.v[3] = (uint8_t)compactRemap[f.v[3]];
    }

    if (history) {
        snap.after = *targetMesh;
        history->Push(std::move(snap));
    }

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(targetLc->data->chunkName, ws);

    const_cast<LocalGeometryOverlay&>(overlay).m_selectedVertices.clear();
    const_cast<LocalGeometryOverlay&>(overlay).m_selectedVertexIdx = -1;
    return true;
}

// ---------------------------------------------------------------------------
// DeleteSelectedVertices
// ---------------------------------------------------------------------------
bool DeleteSelectedVertices(LocalGeometryOverlay& overlay, History* history) {
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

            std::set<int> vertIndices;
            for (const auto& sv : selVerts) {
                if (sv.chunkName == cName && sv.objectIdx == oIdx && sv.meshIdx == mIdx)
                    vertIndices.insert(sv.vertexIdx);
            }

            if (vertIndices.empty()) continue;

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Delete Vertices";
            }

            // Remove faces that reference any deleted vertex
            std::vector<RenderFace> remainingFaces;
            for (const auto& f : mesh.faces) {
                bool referencesDeleted = (vertIndices.find((int)f.v[0]) != vertIndices.end() ||
                                          vertIndices.find((int)f.v[1]) != vertIndices.end() ||
                                          vertIndices.find((int)f.v[2]) != vertIndices.end() ||
                                          (f.v[3] != 0xFF && vertIndices.find((int)f.v[3]) != vertIndices.end()));
                if (!referencesDeleted) {
                    remainingFaces.push_back(f);
                }
            }
            mesh.faces = std::move(remainingFaces);

            // Compact vertices
            std::vector<float> newVx, newVy, newVz;
            std::vector<int> remap(mesh.vx.size(), -1);
            for (size_t i = 0; i < mesh.vx.size(); ++i) {
                if (vertIndices.find((int)i) == vertIndices.end()) {
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
                f.v[0] = (uint8_t)remap[f.v[0]];
                f.v[1] = (uint8_t)remap[f.v[1]];
                f.v[2] = (uint8_t)remap[f.v[2]];
                if (f.v[3] != 0xFF) f.v[3] = (uint8_t)remap[f.v[3]];
            }

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

    if (anyModified) {
        const_cast<LocalGeometryOverlay&>(overlay).m_selectedVertices.clear();
        const_cast<LocalGeometryOverlay&>(overlay).m_selectedVertexIdx = -1;
    }
    return anyModified;
}

} // namespace Geometry
