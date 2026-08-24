#include "geometry/FaceOperations.h"
#include "geometry/MeshOperations.h"
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

static std::string GetWorkspaceDir(const LocalGeometryOverlay& overlay) {
    if (std::filesystem::exists(overlay.m_lastWorkspaceDir))
        return overlay.m_lastWorkspaceDir;
    if (std::filesystem::exists("data/workspace"))
        return "data/workspace";
    if (std::filesystem::exists("../data/workspace"))
        return "../data/workspace";
    return overlay.m_lastWorkspaceDir;
}

static std::set<SelectedFace> GetEffectiveSelectedFaces(const LocalGeometryOverlay& overlay) {
    std::set<SelectedFace> result = overlay.m_selectedFaces;
    if (result.empty() && overlay.m_selectedFaceIdx >= 0 && !overlay.m_selectedChunk.empty() && overlay.m_selectedObjectIdx >= 0) {
        result.insert({overlay.m_selectedChunk, overlay.m_selectedObjectIdx, overlay.m_selectedMeshIdx >= 0 ? overlay.m_selectedMeshIdx : 0, overlay.m_selectedFaceIdx});
    }
    return result;
}

// ---------------------------------------------------------------------------
// TriangulateFaces
// ---------------------------------------------------------------------------
bool TriangulateFaces(LocalGeometryOverlay& overlay, History* history) {
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
            if (lc.data->chunkName != cName || oIdx >= (int)lc.data->objects.size())
                continue;
            auto& obj = lc.data->objects[oIdx];
            if (mIdx >= (int)obj.meshes.size()) break;
            auto& mesh = obj.meshes[mIdx];

            std::set<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.insert(sf.faceIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Triangulate Quads";
            }

            std::vector<RenderFace> newFaces;
            bool meshChanged = false;

            for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                const auto& f = mesh.faces[fi];
                if (faceIndices.find((int)fi) != faceIndices.end() && f.v[3] != 0xFF) {
                    // Tri 1: v0, v1, v2
                    RenderFace t1 = f;
                    t1.v[0] = f.v[0];
                    t1.v[1] = f.v[1];
                    t1.v[2] = f.v[2];
                    t1.v[3] = 0xFF;

                    t1.uv[0][0] = f.uv[0][0]; t1.uv[0][1] = f.uv[0][1];
                    t1.uv[1][0] = f.uv[1][0]; t1.uv[1][1] = f.uv[1][1];
                    t1.uv[2][0] = f.uv[2][0]; t1.uv[2][1] = f.uv[2][1];
                    t1.uv[3][0] = 0.0f;       t1.uv[3][1] = 0.0f;

                    t1.rawU[0] = f.rawU[0]; t1.rawU[1] = f.rawU[1]; t1.rawU[2] = f.rawU[2]; t1.rawU[3] = f.rawU[0];
                    t1.rawV[0] = f.rawV[0]; t1.rawV[1] = f.rawV[1]; t1.rawV[2] = f.rawV[2]; t1.rawV[3] = f.rawV[0];

                    // Tri 2: v0, v2, v3
                    RenderFace t2 = f;
                    t2.v[0] = f.v[0];
                    t2.v[1] = f.v[2];
                    t2.v[2] = f.v[3];
                    t2.v[3] = 0xFF;

                    t2.uv[0][0] = f.uv[0][0]; t2.uv[0][1] = f.uv[0][1];
                    t2.uv[1][0] = f.uv[2][0]; t2.uv[1][1] = f.uv[2][1];
                    t2.uv[2][0] = f.uv[3][0]; t2.uv[2][1] = f.uv[3][1];
                    t2.uv[3][0] = 0.0f;       t2.uv[3][1] = 0.0f;

                    t2.rawU[0] = f.rawU[0]; t2.rawU[1] = f.rawU[2]; t2.rawU[2] = f.rawU[3]; t2.rawU[3] = f.rawU[0];
                    t2.rawV[0] = f.rawV[0]; t2.rawV[1] = f.rawV[2]; t2.rawV[2] = f.rawV[3]; t2.rawV[3] = f.rawV[0];

                    t1.isDirty = true;
newFaces.push_back(t1);
                    t2.isDirty = true;
newFaces.push_back(t2);
                    meshChanged = true;
                } else {
                    newFaces.push_back(f);
                }
            }

            if (meshChanged) {
                mesh.faces = std::move(newFaces);
                anyModified = true;

                if (history) {
                    snap.after = mesh;
                    history->Push(std::move(snap));
                }

                std::string ws = GetWorkspaceDir(overlay);
                overlay.RebuildChunkBatches(cName, ws);
            }
        }
    }

    if (anyModified) {
        const_cast<LocalGeometryOverlay&>(overlay).m_selectedFaces.clear();
        const_cast<LocalGeometryOverlay&>(overlay).m_selectedFaceIdx = -1;
    }
    return anyModified;
}

// ---------------------------------------------------------------------------
// ConnectBridgeFaces
// ---------------------------------------------------------------------------
bool ConnectBridgeFaces(LocalGeometryOverlay& overlay, History* history) {
    auto selFaces = GetEffectiveSelectedFaces(overlay);
    if (selFaces.size() < 2) return false;

    // Look for 2 selected triangles in the same mesh
    std::string cName;
    int oIdx = -1, mIdx = -1;
    std::vector<int> faceIndices;

    for (const auto& sf : selFaces) {
        if (cName.empty()) {
            cName = sf.chunkName; oIdx = sf.objectIdx; mIdx = sf.meshIdx;
        }
        if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx) {
            faceIndices.push_back(sf.faceIdx);
        }
    }

    if (faceIndices.size() < 2) return false;

    for (const auto& lcConst : overlay.GetChunks()) {
        auto& lc = const_cast<LoadedChunk&>(lcConst);
        if (lc.data->chunkName != cName || oIdx >= (int)lc.data->objects.size()) continue;
        auto& obj = lc.data->objects[oIdx];
        if (mIdx >= (int)obj.meshes.size()) break;
        auto& mesh = obj.meshes[mIdx];

        int f1Idx = faceIndices[0];
        int f2Idx = faceIndices[1];
        if (f1Idx >= (int)mesh.faces.size() || f2Idx >= (int)mesh.faces.size()) continue;

        const auto& f1 = mesh.faces[f1Idx];
        const auto& f2 = mesh.faces[f2Idx];

        // Check if both are triangles
        if (f1.v[3] != 0xFF || f2.v[3] != 0xFF) continue;

        // Find shared vertices between f1 (v0,v1,v2) and f2 (v0,v1,v2)
        std::vector<uint8_t> sharedVerts;
        uint8_t unshared1 = 0xFF;
        uint8_t unshared2 = 0xFF;

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

        if (sharedVerts.size() != 2 || unshared1 == 0xFF || unshared2 == 0xFF) continue;

        MeshSnapshot snap;
        if (history) {
            snap.chunkName = cName;
            snap.objectIdx = oIdx;
            snap.meshIdx = mIdx;
            snap.before = mesh;
            snap.description = "Bridge Faces to Quad";
        }

        // Build new Quad: unshared1 -> sharedVerts[0] -> unshared2 -> sharedVerts[1]
        RenderFace quad = f1;
        quad.v[0] = unshared1;
        quad.v[1] = sharedVerts[0];
        quad.v[2] = unshared2;
        quad.v[3] = sharedVerts[1];

        quad.uv[0][0] = 0.0f; quad.uv[0][1] = 0.0f;
        quad.uv[1][0] = 1.0f; quad.uv[1][1] = 0.0f;
        quad.uv[2][0] = 1.0f; quad.uv[2][1] = 1.0f;
        quad.uv[3][0] = 0.0f; quad.uv[3][1] = 1.0f;

        quad.rawU[0] = 0;   quad.rawU[1] = 255; quad.rawU[2] = 255; quad.rawU[3] = 0;
        quad.rawV[0] = 0;   quad.rawV[1] = 0;   quad.rawV[2] = 255; quad.rawV[3] = 255;

        // Erase the two triangles and insert quad
        int maxIdx = std::max(f1Idx, f2Idx);
        int minIdx = std::min(f1Idx, f2Idx);
        mesh.faces.erase(mesh.faces.begin() + maxIdx);
        mesh.faces.erase(mesh.faces.begin() + minIdx);
        quad.isDirty = true;
mesh.faces.push_back(quad);

        if (history) {
            snap.after = mesh;
            history->Push(std::move(snap));
        }

        std::string ws = GetWorkspaceDir(overlay);
        overlay.RebuildChunkBatches(cName, ws);

        const_cast<LocalGeometryOverlay&>(overlay).m_selectedFaces.clear();
        const_cast<LocalGeometryOverlay&>(overlay).m_selectedFaceIdx = -1;
        return true;
    }
    return false;
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

            std::set<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.insert(sf.faceIdx);
            }

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

                // Compute normal
                Vector3 v0 = { mesh.vx[origFace.v[0]], mesh.vy[origFace.v[0]], mesh.vz[origFace.v[0]] };
                Vector3 v1 = { mesh.vx[origFace.v[1]], mesh.vy[origFace.v[1]], mesh.vz[origFace.v[1]] };
                Vector3 v2 = { mesh.vx[origFace.v[2]], mesh.vy[origFace.v[2]], mesh.vz[origFace.v[2]] };
                Vector3 e1 = Vector3Subtract(v1, v0);
                Vector3 e2 = Vector3Subtract(v2, v0);
                Vector3 norm = Vector3Normalize(Vector3CrossProduct(e1, e2));
                if (Vector3Length(norm) < 0.001f) norm = { 0.0f, 1.0f, 0.0f };

                Vector3 offset = Vector3Scale(norm, distance);

                // Create new extruded vertices
                if (mesh.vx.size() + numV > 255) {
                    printf("[Geometry] Vertex limit (255) reached, skipping extrusion for face %d\n", fIdx);
                    continue;
                }

                std::vector<uint8_t> newVIndices;
                for (int i = 0; i < numV; ++i) {
                    uint8_t oldIdx = origFace.v[i];
                    uint8_t newIdx = (uint8_t)mesh.vx.size();
                    mesh.vx.push_back(mesh.vx[oldIdx] + offset.x);
                    mesh.vy.push_back(mesh.vy[oldIdx] + offset.y);
                    mesh.vz.push_back(mesh.vz[oldIdx] + offset.z);
                    newVIndices.push_back(newIdx);
                }

                bool hasTexture = (origFace.texNum != 0x7F && !origFace.texName.empty());
                float minU = 0.0f, maxU = 1.0f, minV = 0.0f, maxV = 1.0f;
                uint8_t minRawU = 0, maxRawU = 255, minRawV = 0, maxRawV = 255;

                if (hasTexture) {
                    minU = origFace.uv[0][0]; maxU = origFace.uv[0][0];
                    minV = origFace.uv[0][1]; maxV = origFace.uv[0][1];
                    minRawU = origFace.rawU[0]; maxRawU = origFace.rawU[0];
                    minRawV = origFace.rawV[0]; maxRawV = origFace.rawV[0];

                    for (int k = 1; k < numV; ++k) {
                        minU = std::min(minU, origFace.uv[k][0]);
                        maxU = std::max(maxU, origFace.uv[k][0]);
                        minV = std::min(minV, origFace.uv[k][1]);
                        maxV = std::max(maxV, origFace.uv[k][1]);
                        minRawU = std::min(minRawU, origFace.rawU[k]);
                        maxRawU = std::max(maxRawU, origFace.rawU[k]);
                        minRawV = std::min(minRawV, origFace.rawV[k]);
                        maxRawV = std::max(maxRawV, origFace.rawV[k]);
                    }
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

            std::set<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.insert(sf.faceIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Invert Normals";
            }

            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                bool isQuad = (face.v[3] != 0xFF);
                if (isQuad) {
                    std::swap(face.v[1], face.v[3]);
                    std::swap(face.uv[1][0], face.uv[3][0]);
                    std::swap(face.uv[1][1], face.uv[3][1]);
                    std::swap(face.rawU[1], face.rawU[3]);
                    std::swap(face.rawV[1], face.rawV[3]);
                } else {
                    std::swap(face.v[0], face.v[2]);
                    std::swap(face.uv[0][0], face.uv[2][0]);
                    std::swap(face.uv[0][1], face.uv[2][1]);
                    std::swap(face.rawU[0], face.rawU[2]);
                    std::swap(face.rawV[0], face.rawV[2]);
                }
                face.isDirty = true;
                changed = true;
            }

            if (changed) {
                if (history) {
                    snap.after = mesh;
                    history->Push(std::move(snap));
                }

                std::string ws = GetWorkspaceDir(overlay);
                overlay.RebuildChunkBatches(cName, ws);
                anyModified = true;
            }
        }
    }
    return anyModified;
}

// ---------------------------------------------------------------------------
// DeleteFaces
// ---------------------------------------------------------------------------
bool DeleteFaces(LocalGeometryOverlay& overlay, bool deleteIsolatedVertices, History* history) {
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

            std::set<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.insert(sf.faceIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = deleteIsolatedVertices ? "Delete Faces & Vertices" : "Delete Faces";
            }

            std::vector<RenderFace> remainingFaces;
            for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                if (faceIndices.find((int)fi) == faceIndices.end()) {
                    remainingFaces.push_back(mesh.faces[fi]);
                }
            }
            mesh.faces = std::move(remainingFaces);

            if (deleteIsolatedVertices) {
                std::vector<bool> vertUsed(mesh.vx.size(), false);
                for (const auto& f : mesh.faces) {
                    vertUsed[f.v[0]] = true;
                    vertUsed[f.v[1]] = true;
                    vertUsed[f.v[2]] = true;
                    if (f.v[3] != 0xFF) vertUsed[f.v[3]] = true;
                }

                std::vector<float> newVx, newVy, newVz;
                std::vector<int> remap(mesh.vx.size(), -1);
                for (size_t vi = 0; vi < mesh.vx.size(); ++vi) {
                    if (vertUsed[vi]) {
                        remap[vi] = (int)newVx.size();
                        newVx.push_back(mesh.vx[vi]);
                        newVy.push_back(mesh.vy[vi]);
                        newVz.push_back(mesh.vz[vi]);
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
        const_cast<LocalGeometryOverlay&>(overlay).m_selectedFaces.clear();
        const_cast<LocalGeometryOverlay&>(overlay).m_selectedFaceIdx = -1;
    }
    return anyModified;
}

// ---------------------------------------------------------------------------
// PaintFaces
// ---------------------------------------------------------------------------
bool PaintFaces(LocalGeometryOverlay& overlay, History* history) {
    if (!overlay.m_texManager) return false;
    const auto& currentTile = overlay.m_texManager->GetCurrentTile();
    if (currentTile.texName.empty()) return false;

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

            std::set<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.insert(sf.faceIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Paint Faces";
            }

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

                for (int r = 0; r < currentTile.rotationSteps; ++r) {
                    float lastU = face.uv[numVerts-1][0];
                    float lastV = face.uv[numVerts-1][1];
                    for (int i = numVerts - 1; i > 0; --i) {
                        face.uv[i][0] = face.uv[i-1][0];
                        face.uv[i][1] = face.uv[i-1][1];
                    }
                    face.uv[0][0] = lastU;
                    face.uv[0][1] = lastV;
                }

                for (int i = 0; i < numVerts; ++i) {
                    face.rawU[i] = (uint8_t)std::clamp((int)std::lroundf(face.uv[i][0] * 255.0f), 0, 255);
                    face.rawV[i] = (uint8_t)std::clamp((int)std::lroundf(face.uv[i][1] * 255.0f), 0, 255);
                }

                face.isDirty = true;

                changed = true;
            }

            if (changed) {
                if (history) {
                    snap.after = mesh;
                    history->Push(std::move(snap));
                }

                std::string ws = GetWorkspaceDir(overlay);
                overlay.RebuildChunkBatches(cName, ws);
                anyModified = true;
            }
        }
    }
    return anyModified;
}

// ---------------------------------------------------------------------------
// ClearTexture
// ---------------------------------------------------------------------------
bool ClearTexture(LocalGeometryOverlay& overlay, History* history) {
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

            std::set<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.insert(sf.faceIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Clear Texture";
            }

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

            if (changed) {
                if (history) {
                    snap.after = mesh;
                    history->Push(std::move(snap));
                }

                std::string ws = GetWorkspaceDir(overlay);
                overlay.RebuildChunkBatches(cName, ws);
                anyModified = true;
            }
        }
    }
    return anyModified;
}

// ---------------------------------------------------------------------------
// RotateUV
// ---------------------------------------------------------------------------
bool RotateUV(LocalGeometryOverlay& overlay, int steps, History* history) {
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

            std::set<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.insert(sf.faceIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Rotate UV";
            }

            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                int numV = (face.v[3] != 0xFF) ? 4 : 3;

                for (int s = 0; s < steps; ++s) {
                    float lastU = face.uv[numV - 1][0];
                    float lastV = face.uv[numV - 1][1];
                    uint8_t lastRawU = face.rawU[numV - 1];
                    uint8_t lastRawV = face.rawV[numV - 1];

                    for (int i = numV - 1; i > 0; --i) {
                        face.uv[i][0] = face.uv[i - 1][0];
                        face.uv[i][1] = face.uv[i - 1][1];
                        face.rawU[i] = face.rawU[i - 1];
                        face.rawV[i] = face.rawV[i - 1];
                    }
                    face.uv[0][0] = lastU;
                    face.uv[0][1] = lastV;
                    face.rawU[0] = lastRawU;
                    face.rawV[0] = lastRawV;
                }
                face.isDirty = true;
                changed = true;
            }

            if (changed) {
                if (history) {
                    snap.after = mesh;
                    history->Push(std::move(snap));
                }

                std::string ws = GetWorkspaceDir(overlay);
                overlay.RebuildChunkBatches(cName, ws);
                anyModified = true;
            }
        }
    }
    return anyModified;
}

// ---------------------------------------------------------------------------
// FlipUV
// ---------------------------------------------------------------------------
bool FlipUV(LocalGeometryOverlay& overlay, bool horizontal, bool vertical, History* history) {
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

            std::set<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.insert(sf.faceIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Flip UV";
            }

            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                int numV = (face.v[3] != 0xFF) ? 4 : 3;

                float minU = 9999.0f, maxU = -9999.0f;
                float minV = 9999.0f, maxV = -9999.0f;
                for (int i = 0; i < numV; ++i) {
                    minU = std::min(minU, face.uv[i][0]); maxU = std::max(maxU, face.uv[i][0]);
                    minV = std::min(minV, face.uv[i][1]); maxV = std::max(maxV, face.uv[i][1]);
                }

                for (int i = 0; i < numV; ++i) {
                    if (horizontal) {
                        face.uv[i][0] = minU + maxU - face.uv[i][0];
                        face.rawU[i] = (uint8_t)std::clamp((int)std::lroundf(face.uv[i][0] * 255.0f), 0, 255);
                    }
                    if (vertical) {
                        face.uv[i][1] = minV + maxV - face.uv[i][1];
                        face.rawV[i] = (uint8_t)std::clamp((int)std::lroundf(face.uv[i][1] * 255.0f), 0, 255);
                    }
                }
                face.isDirty = true;
                changed = true;
            }

            if (changed) {
                if (history) {
                    snap.after = mesh;
                    history->Push(std::move(snap));
                }

                std::string ws = GetWorkspaceDir(overlay);
                overlay.RebuildChunkBatches(cName, ws);
                anyModified = true;
            }
        }
    }
    return anyModified;
}

// ---------------------------------------------------------------------------
// FitUVToTileBounds
// ---------------------------------------------------------------------------
bool FitUVToTileBounds(LocalGeometryOverlay& overlay, History* history) {
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

            std::set<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.insert(sf.faceIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Fit UV to Tile Bounds";
            }

            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                int numV = (face.v[3] != 0xFF) ? 4 : 3;

                float minU = 9999.0f, maxU = -9999.0f;
                float minV = 9999.0f, maxV = -9999.0f;
                for (int i = 0; i < numV; ++i) {
                    minU = std::min(minU, face.uv[i][0]); maxU = std::max(maxU, face.uv[i][0]);
                    minV = std::min(minV, face.uv[i][1]); maxV = std::max(maxV, face.uv[i][1]);
                }

                float rangeU = maxU - minU;
                float rangeV = maxV - minV;
                if (rangeU < 0.0001f) rangeU = 1.0f;
                if (rangeV < 0.0001f) rangeV = 1.0f;

                for (int i = 0; i < numV; ++i) {
                    face.uv[i][0] = (face.uv[i][0] - minU) / rangeU;
                    face.uv[i][1] = (face.uv[i][1] - minV) / rangeV;
                    face.rawU[i] = (uint8_t)std::clamp((int)std::lroundf(face.uv[i][0] * 255.0f), 0, 255);
                    face.rawV[i] = (uint8_t)std::clamp((int)std::lroundf(face.uv[i][1] * 255.0f), 0, 255);
                }
                face.isDirty = true;
                changed = true;
            }

            if (changed) {
                if (history) {
                    snap.after = mesh;
                    history->Push(std::move(snap));
                }

                std::string ws = GetWorkspaceDir(overlay);
                overlay.RebuildChunkBatches(cName, ws);
                anyModified = true;
            }
        }
    }
    return anyModified;
}

// ---------------------------------------------------------------------------
// ResetDefaultUV
// ---------------------------------------------------------------------------
bool ResetDefaultUV(LocalGeometryOverlay& overlay, History* history) {
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

            std::set<int> faceIndices;
            for (const auto& sf : selFaces) {
                if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
                    faceIndices.insert(sf.faceIdx);
            }

            MeshSnapshot snap;
            if (history) {
                snap.chunkName = cName;
                snap.objectIdx = oIdx;
                snap.meshIdx = mIdx;
                snap.before = mesh;
                snap.description = "Reset Default UV";
            }

            bool changed = false;
            for (int fIdx : faceIndices) {
                if (fIdx < 0 || fIdx >= (int)mesh.faces.size()) continue;
                auto& face = mesh.faces[fIdx];
                bool isQuad = (face.v[3] != 0xFF);

                if (isQuad) {
                    face.uv[0][0] = 0.0f; face.uv[0][1] = 0.0f;
                    face.uv[1][0] = 1.0f; face.uv[1][1] = 0.0f;
                    face.uv[2][0] = 1.0f; face.uv[2][1] = 1.0f;
                    face.uv[3][0] = 0.0f; face.uv[3][1] = 1.0f;

                    face.rawU[0] = 0;   face.rawU[1] = 255; face.rawU[2] = 255; face.rawU[3] = 0;
                    face.rawV[0] = 0;   face.rawV[1] = 0;   face.rawV[2] = 255; face.rawV[3] = 255;
                } else {
                    face.uv[0][0] = 0.0f; face.uv[0][1] = 0.0f;
                    face.uv[1][0] = 1.0f; face.uv[1][1] = 0.0f;
                    face.uv[2][0] = 1.0f; face.uv[2][1] = 1.0f;

                    face.rawU[0] = 0;   face.rawU[1] = 255; face.rawU[2] = 255;
                    face.rawV[0] = 0;   face.rawV[1] = 0;   face.rawV[2] = 255;
                }
                face.isDirty = true;
                changed = true;
            }

            if (changed) {
                if (history) {
                    snap.after = mesh;
                    history->Push(std::move(snap));
                }

                std::string ws = GetWorkspaceDir(overlay);
                overlay.RebuildChunkBatches(cName, ws);
                anyModified = true;
            }
        }
    }
    return anyModified;
}

} // namespace Geometry
