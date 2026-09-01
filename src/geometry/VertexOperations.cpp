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

// ---------------------------------------------------------------------------
// SnapVerticesToGrid
// ---------------------------------------------------------------------------
bool SnapVerticesToGrid(LocalGeometryOverlay& overlay, History* history) {
    int rawUnits = 1 << overlay.m_moveStepPower;
    float step = (float)rawUnits / 256.0f;
    if (step <= 0.0001f) step = 1.0f / 256.0f;

    return ForEachSelectedMeshVertices(overlay, history, "Snap Vertices to Grid",
        [step](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& vertIndices) {
            bool changed = false;
            for (int vIdx : vertIndices) {
                if (vIdx < 0 || vIdx >= (int)mesh.vx.size()) continue;
                mesh.vx[vIdx] = std::round(mesh.vx[vIdx] / step) * step;
                mesh.vy[vIdx] = std::round(mesh.vy[vIdx] / step) * step;
                mesh.vz[vIdx] = std::round(mesh.vz[vIdx] / step) * step;
                changed = true;
            }
            return changed;
        });
}

// ---------------------------------------------------------------------------
// SnapVerticesToFloor
// ---------------------------------------------------------------------------
bool SnapVerticesToFloor(LocalGeometryOverlay& overlay, History* history) {
    return ForEachSelectedMeshVertices(overlay, history, "Snap Vertices to Floor",
        [&overlay](LoadedChunk& lc, RenderObject&, RenderMesh& mesh, const std::vector<int>& vertIndices) {
            bool changed = false;
            for (int vIdx : vertIndices) {
                if (vIdx < 0 || vIdx >= (int)mesh.vx.size()) continue;
                float flrY = FindFloorHeightBelow(overlay, mesh.vx[vIdx], mesh.vz[vIdx], mesh.vy[vIdx], lc.data->chunkName, -1);
                mesh.vy[vIdx] = (flrY > -90000.0f) ? flrY : 0.0f;
                changed = true;
            }
            return changed;
        });
}

// ---------------------------------------------------------------------------
// PlanarizeVertices
// ---------------------------------------------------------------------------
bool PlanarizeVertices(LocalGeometryOverlay& overlay, int axis, History* history) {
    const char* desc = (axis == 0) ? "Flatten X" : (axis == 1 ? "Flatten Y" : "Flatten Z");
    return ForEachSelectedMeshVertices(overlay, history, desc,
        [axis](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& vertIndices) {
            double sum = 0.0;
            int count = 0;
            for (int vIdx : vertIndices) {
                if (vIdx < 0 || vIdx >= (int)mesh.vx.size()) continue;
                if (axis == 0) sum += mesh.vx[vIdx];
                else if (axis == 1) sum += mesh.vy[vIdx];
                else if (axis == 2) sum += mesh.vz[vIdx];
                count++;
            }

            if (count == 0) return false;
            float avg = (float)(sum / count);

            for (int vIdx : vertIndices) {
                if (vIdx < 0 || vIdx >= (int)mesh.vx.size()) continue;
                if (axis == 0) mesh.vx[vIdx] = avg;
                else if (axis == 1) mesh.vy[vIdx] = avg;
                else if (axis == 2) mesh.vz[vIdx] = avg;
            }
            return true;
        });
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

    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    RenderMesh* mesh = nullptr;
    if (!GetActiveMeshTarget(overlay, lc, obj, mesh) || !lc || !obj || !mesh) return false;

    int mIdx = overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0;
    std::vector<int> vertIndices;
    for (const auto& sv : selVerts) {
        if (sv.chunkName == lc->data->chunkName && sv.objectIdx == overlay.m_selectedObjectIdx && sv.meshIdx == mIdx) {
            vertIndices.push_back(sv.vertexIdx);
        }
    }
    if (vertIndices.size() != 3 && vertIndices.size() != 4) return false;

    MeshSnapshot snap;
    if (history) {
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = mIdx;
        snap.before = *mesh;
        snap.description = "Add Face from Vertices";
    }

    std::vector<uint8_t> sorted = (vertIndices.size() == 4) ? SortVerticesByAngle(*mesh, vertIndices)
                                                             : std::vector<uint8_t>(vertIndices.begin(), vertIndices.end());

    const RenderFace* inheritFace = mesh->faces.empty() ? nullptr : &mesh->faces[0];
    RenderFace newFace = CreateDefaultFace(*obj, mIdx, sorted, inheritFace);
    mesh->faces.push_back(newFace);

    if (history) {
        snap.after = *mesh;
        history->Push(std::move(snap));
    }

    RecalculateBounds(overlay);
    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);
    return true;
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
                uint8_t origV[4] = { origFace.v[0], origFace.v[1], origFace.v[2], origFace.v[3] };

                Vector3 p0 = GetMeshVertex(mesh, origV[0]);
                Vector3 p1 = GetMeshVertex(mesh, origV[1]);
                Vector3 p2 = GetMeshVertex(mesh, origV[2]);
                Vector3 p3 = GetMeshVertex(mesh, origV[3]);

                Vector3 norm = ComputeTriangleNormal(p0, p1, p2);
                Vector3 finalOffset = Vector3Scale(norm, step);

                uint8_t newV[4] = {
                    AddMeshVertex(mesh, Vector3Add(p0, finalOffset)),
                    AddMeshVertex(mesh, Vector3Add(p1, finalOffset)),
                    AddMeshVertex(mesh, Vector3Add(p2, finalOffset)),
                    AddMeshVertex(mesh, Vector3Add(p3, finalOffset))
                };

                bool hasTexture = (origFace.texNum != 0x7F && !origFace.texName.empty());
                float minU = 0.0f, maxU = 1.0f, minV = 0.0f, maxV = 1.0f;
                uint8_t minRawU = 0, maxRawU = 255, minRawV = 0, maxRawV = 255;

                if (hasTexture) {
                    ComputeUvBounds(origFace.uv, 4, minU, maxU, minV, maxV);
                    minRawU = NormalizedToByteUv(minU);
                    maxRawU = NormalizedToByteUv(maxU);
                    minRawV = NormalizedToByteUv(minV);
                    maxRawV = NormalizedToByteUv(maxV);
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
                capFace.v[0] = newV[0];
                capFace.v[1] = newV[1];
                capFace.v[2] = newV[2];
                capFace.v[3] = newV[3];
                newFaces.push_back(capFace);

                // Remove the original face
                mesh.faces.erase(mesh.faces.begin() + foundFaceIdx);

                // Add the 5 new faces
                for (const auto& nf : newFaces) {
                    mesh.faces.push_back(nf);
                }

                for (int i = 0; i < 4; ++i) newGlobalSelectedVerts.insert({ cName, oIdx, mIdx, newV[i] });
                if (newPrimarySelectedVertex < 0) newPrimarySelectedVertex = newV[0];
                lastChunkName = cName; lastObjIdx = oIdx; lastMeshIdx = mIdx;
            } else {
                // Duplicate vertices along offset vector
                std::map<uint8_t, uint8_t> oldToNew;
                for (int vi : vertIndices) {
                    uint8_t newIdx = AddMeshVertex(mesh, Vector3Add(GetMeshVertex(mesh, vi), offset));
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
                            if (oldToNew.count(a) && oldToNew.count(b)) {
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

    LoadedChunk* targetLc = nullptr;
    RenderObject* targetObj = nullptr;
    RenderMesh* targetMesh = nullptr;
    if (!GetActiveMeshTarget(overlay, targetLc, targetObj, targetMesh) || !targetMesh || !targetLc) return false;

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
        Vector3 vi = GetMeshVertex(*targetMesh, i);
        for (size_t j = i + 1; j < numV; ++j) {
            if (remap[j] != (int)j) continue;
            Vector3 vj = GetMeshVertex(*targetMesh, j);
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
    CompactMeshUnusedVertices(*targetMesh);

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
    bool anyModified = ForEachSelectedMeshVertices(overlay, history, "Delete Vertices",
        [](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& vertIndices) {
            std::set<int> delSet(vertIndices.begin(), vertIndices.end());
            std::vector<RenderFace> remainingFaces;
            for (const auto& f : mesh.faces) {
                bool referencesDeleted = (delSet.count(f.v[0]) || delSet.count(f.v[1]) || delSet.count(f.v[2]) ||
                                          (f.v[3] != 0xFF && delSet.count(f.v[3])));
                if (!referencesDeleted) {
                    remainingFaces.push_back(f);
                }
            }
            mesh.faces = std::move(remainingFaces);
            CompactMeshUnusedVertices(mesh);
            return true;
        });

    if (anyModified) {
        auto& mutableOverlay = const_cast<LocalGeometryOverlay&>(overlay);
        mutableOverlay.m_selectedVertices.clear();
        mutableOverlay.m_selectedVertexIdx = -1;
    }
    return anyModified;
}

} // namespace Geometry
