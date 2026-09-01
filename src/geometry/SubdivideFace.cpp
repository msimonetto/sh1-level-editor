#include "geometry/SubdivideFace.h"
#include "geometry/GeometryCommon.h"
#include "geometry/TransformOperations.h"
#include "viewport/LocalGeometryOverlay.h"
#include "core/History.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <vector>

namespace Geometry {

void SubdivideSelectedFaces(LocalGeometryOverlay& vp) {
    if (vp.m_editMode != EditMode::Face || vp.m_selectedFaces.empty())
        return;

    constexpr float TILE_SIZE = 1.0f; // Target tile size in world units (1 grid square)
    constexpr float MIN_TILE  = 0.3f; // Min remainder that gets its own tile
    constexpr float MIN_WIDTH = 0.3f; // Min width of non-split dimension

    int totalFacesSubdivided = 0;

    ForEachSelectedMeshFaces(vp, vp.m_history, "Subdivide Faces",
        [&](LoadedChunk&, RenderObject& obj, RenderMesh& mesh, const std::vector<int>& faceIndices) {
            std::set<int> selectedSet(faceIndices.begin(), faceIndices.end());
            std::vector<RenderFace> newFaces;
            bool meshChanged = false;

            for (int fIdx = 0; fIdx < (int)mesh.faces.size(); ++fIdx) {
                const RenderFace& face = mesh.faces[fIdx];

                if (!selectedSet.count(fIdx) || face.v[3] == 0xFF) {
                    newFaces.push_back(face);
                    continue;
                }

                // v0 (TL), v1 (BL), v2 (BR), v3 (TR) in RenderFace winding order
                Vector3 v0 = GetMeshVertex(mesh, face.v[0]);
                Vector3 v1 = GetMeshVertex(mesh, face.v[1]);
                Vector3 v2 = GetMeshVertex(mesh, face.v[2]);
                Vector3 v3 = GetMeshVertex(mesh, face.v[3]);

                float len01 = Vector3Distance(v0, v1);
                float len23 = Vector3Distance(v2, v3);
                float len12 = Vector3Distance(v1, v2);
                float len30 = Vector3Distance(v3, v0);

                float lenVert  = (len01 + len23) * 0.5f;
                float lenHoriz = (len12 + len30) * 0.5f;

                bool tryVert  = (lenVert >= lenHoriz) && (lenHoriz >= MIN_WIDTH) && (lenVert > TILE_SIZE);
                bool tryHoriz = (lenHoriz > lenVert)  && (lenVert >= MIN_WIDTH)  && (lenHoriz > TILE_SIZE);

                if (!tryVert && !tryHoriz) {
                    newFaces.push_back(face);
                    continue;
                }

                bool splitVert = tryVert;
                float L = splitVert ? lenVert : lenHoriz;

                int fullTiles = (int)floorf(L / TILE_SIZE);
                float remainder = L - fullTiles * TILE_SIZE;
                int N = fullTiles + (remainder >= MIN_TILE ? 1 : 0);

                if (N <= 1) {
                    newFaces.push_back(face);
                    continue;
                }

                int maxNewVerts = (N - 1) * 2;
                if (mesh.vx.size() + maxNewVerts > 255) {
                    printf("[Subdivide] WARNING: Face %d skipped! Mesh '%s' vertex count "
                           "(%zu + %d) would exceed PS1 max limit of 255 vertices.\n",
                           fIdx, obj.name.c_str(), mesh.vx.size(), maxNewVerts);
                    newFaces.push_back(face);
                    continue;
                }

                std::vector<float> ts;
                ts.reserve(N + 1);
                ts.push_back(0.0f);
                for (int i = 1; i < N; ++i) {
                    ts.push_back((i * TILE_SIZE) / L);
                }
                ts.push_back(1.0f);

                Vector3 eA0, eA1, eB0, eB1;
                float uvA0u, uvA0v, uvA1u, uvA1v;
                float uvB0u, uvB0v, uvB1u, uvB1v;

                if (splitVert) {
                    eA0 = v0; eA1 = v1; eB0 = v3; eB1 = v2;
                    uvA0u = face.uv[0][0]; uvA0v = face.uv[0][1];
                    uvA1u = face.uv[1][0]; uvA1v = face.uv[1][1];
                    uvB0u = face.uv[3][0]; uvB0v = face.uv[3][1];
                    uvB1u = face.uv[2][0]; uvB1v = face.uv[2][1];
                } else {
                    eA0 = v0; eA1 = v3; eB0 = v1; eB1 = v2;
                    uvA0u = face.uv[0][0]; uvA0v = face.uv[0][1];
                    uvA1u = face.uv[3][0]; uvA1v = face.uv[3][1];
                    uvB0u = face.uv[1][0]; uvB0v = face.uv[1][1];
                    uvB1u = face.uv[2][0]; uvB1v = face.uv[2][1];
                }

                size_t initialVxCount = mesh.vx.size();
                bool capacityExceeded = false;

                auto getOrCreate = [&](Vector3 pos) -> uint8_t {
                    for (size_t k = 0; k < mesh.vx.size(); ++k) {
                        if (Vector3DistanceSqr(GetMeshVertex(mesh, k), pos) < 0.000001f)
                            return (uint8_t)std::min(k, (size_t)254);
                    }
                    if (mesh.vx.size() >= 255) {
                        capacityExceeded = true;
                        return 254;
                    }
                    return AddMeshVertex(mesh, pos);
                };

                auto lerpf = [](float a, float b, float t) { return a + (b - a) * t; };

                std::vector<RenderFace> emittedQuads;

                for (int seg = 0; seg < N; ++seg) {
                    float t0 = ts[seg], t1 = ts[seg + 1];

                    Vector3 pA0 = Vector3Lerp(eA0, eA1, t0);
                    Vector3 pA1 = Vector3Lerp(eA0, eA1, t1);
                    Vector3 pB0 = Vector3Lerp(eB0, eB1, t0);
                    Vector3 pB1 = Vector3Lerp(eB0, eB1, t1);

                    float uA0 = lerpf(uvA0u, uvA1u, t0), vA0 = lerpf(uvA0v, uvA1v, t0);
                    float uA1 = lerpf(uvA0u, uvA1u, t1), vA1 = lerpf(uvA0v, uvA1v, t1);
                    float uB0 = lerpf(uvB0u, uvB1u, t0), vB0 = lerpf(uvB0v, uvB1v, t0);
                    float uB1 = lerpf(uvB0u, uvB1u, t1), vB1 = lerpf(uvB0v, uvB1v, t1);

                    RenderFace sub = face;
                    if (splitVert) {
                        sub.v[0] = getOrCreate(pA0); sub.uv[0][0] = uA0; sub.uv[0][1] = vA0;
                        sub.v[1] = getOrCreate(pA1); sub.uv[1][0] = uA1; sub.uv[1][1] = vA1;
                        sub.v[2] = getOrCreate(pB1); sub.uv[2][0] = uB1; sub.uv[2][1] = vB1;
                        sub.v[3] = getOrCreate(pB0); sub.uv[3][0] = uB0; sub.uv[3][1] = vB0;
                    } else {
                        sub.v[0] = getOrCreate(pA0); sub.uv[0][0] = uA0; sub.uv[0][1] = vA0;
                        sub.v[1] = getOrCreate(pB0); sub.uv[1][0] = uB0; sub.uv[1][1] = vB0;
                        sub.v[2] = getOrCreate(pB1); sub.uv[2][0] = uB1; sub.uv[2][1] = vB1;
                        sub.v[3] = getOrCreate(pA1); sub.uv[3][0] = uA1; sub.uv[3][1] = vA1;
                    }

                    if (capacityExceeded) break;

                    for (int k = 0; k < 4; ++k) {
                        sub.rawU[k] = NormalizedToByteUv(sub.uv[k][0]);
                        sub.rawV[k] = NormalizedToByteUv(sub.uv[k][1]);
                    }
                    sub.isDirty = true;
                    emittedQuads.push_back(sub);
                }

                if (capacityExceeded) {
                    mesh.vx.resize(initialVxCount);
                    mesh.vy.resize(initialVxCount);
                    mesh.vz.resize(initialVxCount);
                    newFaces.push_back(face);
                    printf("[Subdivide] WARNING: Face %d skipped on mesh '%s': "
                           "255-vertex capacity exceeded during quad generation.\n",
                           fIdx, obj.name.c_str());
                    continue;
                }

                for (const auto& q : emittedQuads)
                    newFaces.push_back(q);

                totalFacesSubdivided++;
                meshChanged = true;
            }

            if (meshChanged) {
                mesh.faces = std::move(newFaces);
            }
            return meshChanged;
        }, true);

    if (totalFacesSubdivided > 0) {
        printf("[Geometry] Subdivided %d faces\n", totalFacesSubdivided);
    } else {
        printf("[Geometry] Subdivide: no eligible quads found (>1.0 world units on "
               "longest edge, width>=0.3 units, verts<=255)\n");
    }
}

} // namespace Geometry
