#include "geometry/FaceOperations.h"
#include "geometry/MeshOperations.h"
#include "geometry/TransformOperations.h"
#include "geometry/GeometryCommon.h"
#include "viewport/LocalGeometryOverlay.h"
#include "panels/TextureMapPanel.h"
#include "core/History.h"
#include "formats/IPDWrite.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace Geometry {

// ---------------------------------------------------------------------------
// TriangulateFaces
// ---------------------------------------------------------------------------
bool TriangulateFaces(LocalGeometryOverlay& overlay, History* history) {
    return ForEachSelectedMeshFaces(overlay, history, "Triangulate Quads",
        [](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& faceIndices) {
            std::set<int> fSet(faceIndices.begin(), faceIndices.end());
            std::vector<RenderFace> newFaces;
            bool changed = false;

            for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                const auto& f = mesh.faces[fi];
                if (fSet.count((int)fi) && f.v[3] != 0xFF) {
                    // Tri 1: v0, v1, v2
                    RenderFace t1 = f;
                    t1.v[0] = f.v[0]; t1.v[1] = f.v[1]; t1.v[2] = f.v[2]; t1.v[3] = 0xFF;
                    t1.uv[0][0] = f.uv[0][0]; t1.uv[0][1] = f.uv[0][1];
                    t1.uv[1][0] = f.uv[1][0]; t1.uv[1][1] = f.uv[1][1];
                    t1.uv[2][0] = f.uv[2][0]; t1.uv[2][1] = f.uv[2][1];
                    t1.uv[3][0] = 0.0f;       t1.uv[3][1] = 0.0f;
                    t1.rawU[0] = f.rawU[0]; t1.rawU[1] = f.rawU[1]; t1.rawU[2] = f.rawU[2]; t1.rawU[3] = f.rawU[0];
                    t1.rawV[0] = f.rawV[0]; t1.rawV[1] = f.rawV[1]; t1.rawV[2] = f.rawV[2]; t1.rawV[3] = f.rawV[0];
                    t1.isDirty = true;
                    newFaces.push_back(t1);

                    // Tri 2: v0, v2, v3
                    RenderFace t2 = f;
                    t2.v[0] = f.v[0]; t2.v[1] = f.v[2]; t2.v[2] = f.v[3]; t2.v[3] = 0xFF;
                    t2.uv[0][0] = f.uv[0][0]; t2.uv[0][1] = f.uv[0][1];
                    t2.uv[1][0] = f.uv[2][0]; t2.uv[1][1] = f.uv[2][1];
                    t2.uv[2][0] = f.uv[3][0]; t2.uv[2][1] = f.uv[3][1];
                    t2.uv[3][0] = 0.0f;       t2.uv[3][1] = 0.0f;
                    t2.rawU[0] = f.rawU[0]; t2.rawU[1] = f.rawU[2]; t2.rawU[2] = f.rawU[3]; t2.rawU[3] = f.rawU[0];
                    t2.rawV[0] = f.rawV[0]; t2.rawV[1] = f.rawV[2]; t2.rawV[2] = f.rawV[3]; t2.rawV[3] = f.rawV[0];
                    t2.isDirty = true;
                    newFaces.push_back(t2);
                    changed = true;
                } else {
                    newFaces.push_back(f);
                }
            }
            if (changed) mesh.faces = std::move(newFaces);
            return changed;
        }, true);
}

// ---------------------------------------------------------------------------
// ConnectBridgeFaces
// ---------------------------------------------------------------------------
bool ConnectBridgeFaces(LocalGeometryOverlay& overlay, History* history) {
    auto selFaces = GetEffectiveSelectedFaces(overlay);
    if (selFaces.size() < 2) return false;

    LoadedChunk* lc = nullptr;
    RenderObject* obj = nullptr;
    RenderMesh* mesh = nullptr;
    if (!GetActiveMeshTarget(overlay, lc, obj, mesh) || !lc || !mesh) return false;

    int mIdx = overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0;
    std::vector<int> faceIndices;
    for (const auto& sf : selFaces) {
        if (sf.chunkName == lc->data->chunkName && sf.objectIdx == overlay.m_selectedObjectIdx && sf.meshIdx == mIdx) {
            faceIndices.push_back(sf.faceIdx);
        }
    }
    if (faceIndices.size() < 2) return false;

    int f1Idx = faceIndices[0];
    int f2Idx = faceIndices[1];
    if (f1Idx >= (int)mesh->faces.size() || f2Idx >= (int)mesh->faces.size()) return false;

    const auto& f1 = mesh->faces[f1Idx];
    const auto& f2 = mesh->faces[f2Idx];
    if (f1.v[3] != 0xFF || f2.v[3] != 0xFF) return false;

    std::vector<uint8_t> sharedVerts;
    uint8_t unshared1 = 0xFF, unshared2 = 0xFF;

    for (int i = 0; i < 3; ++i) {
        uint8_t v = f1.v[i];
        bool shared = false;
        for (int j = 0; j < 3; ++j) {
            if (v == f2.v[j]) { shared = true; break; }
        }
        if (shared) sharedVerts.push_back(v);
        else unshared1 = v;
    }

    for (int j = 0; j < 3; ++j) {
        uint8_t v = f2.v[j];
        bool shared = false;
        for (int i = 0; i < 3; ++i) {
            if (v == f1.v[i]) { shared = true; break; }
        }
        if (!shared) unshared2 = v;
    }

    if (sharedVerts.size() != 2 || unshared1 == 0xFF || unshared2 == 0xFF) return false;

    MeshSnapshot snap;
    if (history) {
        snap.chunkName = lc->data->chunkName;
        snap.objectIdx = overlay.m_selectedObjectIdx;
        snap.meshIdx = mIdx;
        snap.before = *mesh;
        snap.description = "Bridge Faces to Quad";
    }

    RenderFace quad = f1;
    quad.v[0] = unshared1;
    quad.v[1] = sharedVerts[0];
    quad.v[2] = unshared2;
    quad.v[3] = sharedVerts[1];
    ResetFaceDefaultUV(quad.uv, quad.rawU, quad.rawV, 4);

    int maxIdx = std::max(f1Idx, f2Idx);
    int minIdx = std::min(f1Idx, f2Idx);
    mesh->faces.erase(mesh->faces.begin() + maxIdx);
    mesh->faces.erase(mesh->faces.begin() + minIdx);
    quad.isDirty = true;
    mesh->faces.push_back(quad);

    if (history) {
        snap.after = *mesh;
        history->Push(std::move(snap));
    }

    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(lc->data->chunkName, ws);

    auto& mutableOverlay = const_cast<LocalGeometryOverlay&>(overlay);
    mutableOverlay.m_selectedFaces.clear();
    mutableOverlay.m_selectedFaceIdx = -1;
    return true;
}

// ---------------------------------------------------------------------------
// ExtrudeFaces
// ---------------------------------------------------------------------------
bool ExtrudeFaces(LocalGeometryOverlay& overlay, float distance, int mode, History* history) {
    auto selFaces = GetEffectiveSelectedFaces(overlay);
    if (selFaces.empty()) return false;

    if (distance <= 0.001f) {
        int rawUnits = 1 << overlay.m_moveStepPower;
        distance = (float)rawUnits / 256.0f;
    }

    std::set<std::tuple<std::string, int, int>> modifiedMeshes;
    for (const auto& sf : selFaces) {
        modifiedMeshes.insert({sf.chunkName, sf.objectIdx, sf.meshIdx});
    }

    bool anyModified = false;
    std::set<SelectedFace> newGlobalSelectedFaces;
    int newPrimarySelectedFace = -1;
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
                snap.description = (mode == 0) ? "Extrude Faces" : "Extrude & Separate Faces";
            }

            std::vector<RenderFace> sideQuads;

            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                const RenderFace origFace = mesh.faces[fIdx];

                bool isQuad = (origFace.v[3] != 0xFF);
                int numV = isQuad ? 4 : 3;

                Vector3 norm = ComputeFaceNormal(mesh, origFace);
                Vector3 offset = Vector3Scale(norm, distance);

                if (mesh.vx.size() + numV > 255) {
                    printf("[Geometry] Vertex limit (255) reached, skipping extrusion for face %d\n", fIdx);
                    continue;
                }

                std::vector<uint8_t> newVIndices;
                for (int i = 0; i < numV; ++i) {
                    uint8_t oldIdx = origFace.v[i];
                    uint8_t newIdx = AddMeshVertex(mesh, Vector3Add(GetMeshVertex(mesh, oldIdx), offset));
                    newVIndices.push_back(newIdx);
                }

                bool hasTexture = (origFace.texNum != 0x7F && !origFace.texName.empty());
                float minU = 0.0f, maxU = 1.0f, minV = 0.0f, maxV = 1.0f;
                uint8_t minRawU = 0, maxRawU = 255, minRawV = 0, maxRawV = 255;

                if (hasTexture) {
                    ComputeUvBounds(origFace.uv, numV, minU, maxU, minV, maxV);
                    minRawU = NormalizedToByteUv(minU);
                    maxRawU = NormalizedToByteUv(maxU);
                    minRawV = NormalizedToByteUv(minV);
                    maxRawV = NormalizedToByteUv(maxV);
                }

                // If mode 0: create side quad faces (outward facing)
                if (mode == 0) {
                    for (int i = 0; i < numV; ++i) {
                        int nextI = (i + 1) % numV;
                        RenderFace side = origFace;
                        side.v[0] = origFace.v[i];
                        side.v[1] = origFace.v[nextI];
                        side.v[2] = newVIndices[nextI];
                        side.v[3] = newVIndices[i];

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

                        side.isDirty = true;
                        sideQuads.push_back(side);
                    }
                }

                // Create the cap face across from original face, preserving rotation/mirroring
                RenderFace capFace = origFace;
                for (int i = 0; i < numV; ++i) {
                    capFace.v[i] = newVIndices[i];
                }
                if (!isQuad) {
                    capFace.v[3] = 0xFF;
                }

                // Replace the original face with the new cap face
                capFace.isDirty = true;
                mesh.faces[fIdx] = capFace;

                newGlobalSelectedFaces.insert({ cName, oIdx, mIdx, fIdx });
                if (newPrimarySelectedFace < 0) newPrimarySelectedFace = fIdx;
                lastChunkName = cName; lastObjIdx = oIdx; lastMeshIdx = mIdx;
            }

            for (const auto& sq : sideQuads) {
                mesh.faces.push_back(sq);
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

    if (anyModified && !newGlobalSelectedFaces.empty()) {
        auto& mutableOverlay = const_cast<LocalGeometryOverlay&>(overlay);
        mutableOverlay.m_selectedFaces = newGlobalSelectedFaces;
        mutableOverlay.m_selectedFaceIdx = newPrimarySelectedFace;
        if (!lastChunkName.empty()) {
            mutableOverlay.m_selectedChunk = lastChunkName;
            mutableOverlay.m_selectedObjectIdx = lastObjIdx;
            mutableOverlay.m_selectedMeshIdx = lastMeshIdx;
        }
    }

    return anyModified;
}

// ---------------------------------------------------------------------------
// InvertNormals
// ---------------------------------------------------------------------------
bool InvertNormals(LocalGeometryOverlay& overlay, History* history) {
    return ForEachSelectedMeshFaces(overlay, history, "Invert Normals",
        [](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& faceIndices) {
            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                InvertPolygonWinding(face.v, face.uv, face.rawU, face.rawV);
                face.isDirty = true;
                changed = true;
            }
            return changed;
        });
}

// ---------------------------------------------------------------------------
// DeleteFaces
// ---------------------------------------------------------------------------
bool DeleteFaces(LocalGeometryOverlay& overlay, bool deleteIsolatedVertices, History* history) {
    const char* desc = deleteIsolatedVertices ? "Delete Faces & Vertices" : "Delete Faces";
    return ForEachSelectedMeshFaces(overlay, history, desc,
        [deleteIsolatedVertices](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& faceIndices) {
            std::set<int> delSet(faceIndices.begin(), faceIndices.end());
            std::vector<RenderFace> remainingFaces;
            for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                if (!delSet.count((int)fi)) {
                    remainingFaces.push_back(mesh.faces[fi]);
                }
            }
            mesh.faces = std::move(remainingFaces);
            if (deleteIsolatedVertices) {
                CompactMeshUnusedVertices(mesh);
            }
            return true;
        }, true, deleteIsolatedVertices);
}

// ---------------------------------------------------------------------------
// PaintFaces
// ---------------------------------------------------------------------------
bool PaintFaces(LocalGeometryOverlay& overlay, History* history) {
    if (!overlay.m_texManager) return false;
    const auto& currentTile = overlay.m_texManager->GetCurrentTile();
    if (currentTile.texName.empty()) return false;

    return ForEachSelectedMeshFaces(overlay, history, "Paint Faces",
        [&currentTile](LoadedChunk& lc, RenderObject& obj, RenderMesh& mesh, const std::vector<int>& faceIndices) {
            uint8_t texNum = 0x7F;
            const auto& texList = obj.isGlobal ? lc.data->globalTexNames : lc.data->localTexNames;
            for (size_t i = 0; i < texList.size(); i++) {
                if (texList[i] == currentTile.texName) {
                    texNum = (uint8_t)i;
                    break;
                }
            }

            float minU = currentTile.minU; float minV = currentTile.minV;
            float maxU = currentTile.maxU; float maxV = currentTile.maxV;

            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];

                face.texName = currentTile.texName;
                face.texNum = texNum;
                face.paletteRow = currentTile.palette;
                face.cbaRaw = (face.cbaRaw & ~0x7FC0u) | (((uint16_t)face.paletteRow & 0xFF) << 6);

                int numVerts = (face.v[3] != 0xFF) ? 4 : 3;
                if (numVerts == 4) {
                    face.uv[0][0] = minU; face.uv[0][1] = minV;
                    face.uv[1][0] = maxU; face.uv[1][1] = minV;
                    face.uv[2][0] = maxU; face.uv[2][1] = maxV;
                    face.uv[3][0] = minU; face.uv[3][1] = maxV;
                } else {
                    face.uv[0][0] = minU; face.uv[0][1] = minV;
                    face.uv[1][0] = maxU; face.uv[1][1] = minV;
                    face.uv[2][0] = maxU; face.uv[2][1] = maxV;
                }

                for (int i = 0; i < numVerts; ++i) {
                    face.rawU[i] = NormalizedToByteUv(face.uv[i][0]);
                    face.rawV[i] = NormalizedToByteUv(face.uv[i][1]);
                }

                if (currentTile.rotationSteps > 0) {
                    RotatePolygonUv(face.uv, face.rawU, face.rawV, numVerts, currentTile.rotationSteps);
                }

                face.isDirty = true;
                changed = true;
            }
            return changed;
        });
}

// ---------------------------------------------------------------------------
// ClearTexture
// ---------------------------------------------------------------------------
bool ClearTexture(LocalGeometryOverlay& overlay, History* history) {
    return ForEachSelectedMeshFaces(overlay, history, "Clear Texture",
        [](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& faceIndices) {
            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                face.texName = "";
                face.texNum = 0x7F;
                face.paletteRow = 0;
                face.isDirty = true;
                changed = true;
            }
            return changed;
        });
}

// ---------------------------------------------------------------------------
// RotateUV
// ---------------------------------------------------------------------------
bool RotateUV(LocalGeometryOverlay& overlay, int steps, History* history) {
    return ForEachSelectedMeshFaces(overlay, history, "Rotate UV",
        [steps](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& faceIndices) {
            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                int numV = (face.v[3] != 0xFF) ? 4 : 3;
                RotatePolygonUv(face.uv, face.rawU, face.rawV, numV, steps);
                face.isDirty = true;
                changed = true;
            }
            return changed;
        });
}

// ---------------------------------------------------------------------------
// FlipUV
// ---------------------------------------------------------------------------
bool FlipUV(LocalGeometryOverlay& overlay, bool horizontal, bool vertical, History* history) {
    return ForEachSelectedMeshFaces(overlay, history, "Flip UV",
        [horizontal, vertical](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& faceIndices) {
            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                int numV = (face.v[3] != 0xFF) ? 4 : 3;

                float minU, maxU, minV, maxV;
                ComputeUvBounds(face.uv, numV, minU, maxU, minV, maxV);

                for (int i = 0; i < numV; ++i) {
                    if (horizontal) {
                        face.uv[i][0] = minU + maxU - face.uv[i][0];
                        face.rawU[i] = NormalizedToByteUv(face.uv[i][0]);
                    }
                    if (vertical) {
                        face.uv[i][1] = minV + maxV - face.uv[i][1];
                        face.rawV[i] = NormalizedToByteUv(face.uv[i][1]);
                    }
                }
                face.isDirty = true;
                changed = true;
            }
            return changed;
        });
}

// ---------------------------------------------------------------------------
// FitUVToTileBounds
// ---------------------------------------------------------------------------
bool FitUVToTileBounds(LocalGeometryOverlay& overlay, History* history) {
    return ForEachSelectedMeshFaces(overlay, history, "Fit UV to Tile Bounds",
        [](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& faceIndices) {
            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                int numV = (face.v[3] != 0xFF) ? 4 : 3;

                float minU, maxU, minV, maxV;
                ComputeUvBounds(face.uv, numV, minU, maxU, minV, maxV);

                float rangeU = maxU - minU;
                float rangeV = maxV - minV;
                if (rangeU < 0.0001f) rangeU = 1.0f;
                if (rangeV < 0.0001f) rangeV = 1.0f;

                for (int i = 0; i < numV; ++i) {
                    face.uv[i][0] = (face.uv[i][0] - minU) / rangeU;
                    face.uv[i][1] = (face.uv[i][1] - minV) / rangeV;
                    face.rawU[i] = NormalizedToByteUv(face.uv[i][0]);
                    face.rawV[i] = NormalizedToByteUv(face.uv[i][1]);
                }
                face.isDirty = true;
                changed = true;
            }
            return changed;
        });
}

// ---------------------------------------------------------------------------
// ResetDefaultUV
// ---------------------------------------------------------------------------
bool ResetDefaultUV(LocalGeometryOverlay& overlay, History* history) {
    return ForEachSelectedMeshFaces(overlay, history, "Reset Default UV",
        [](LoadedChunk&, RenderObject&, RenderMesh& mesh, const std::vector<int>& faceIndices) {
            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                int numV = (face.v[3] != 0xFF) ? 4 : 3;
                ResetFaceDefaultUV(face.uv, face.rawU, face.rawV, numV);
                face.isDirty = true;
                changed = true;
            }
            return changed;
        });
}

} // namespace Geometry
