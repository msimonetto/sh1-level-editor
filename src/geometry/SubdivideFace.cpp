#include "geometry/SubdivideFace.h"
#include "viewport/LocalGeometry.h"
#include "core/History.h"
#include "formats/IPDWrite.h"
#include "raymath.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <vector>

namespace Geometry {

void SubdivideSelectedFaces(LocalGeometryOverlay& vp) {
  if (vp.m_editMode != EditMode::Face || vp.m_selectedFaces.empty())
    return;

  // --- Tunables ---
  // Vertex positions are in world units where 1 world unit = 256 raw IPD units.
  // One grid square across the 40x40 XZ chunk grid = 1.0 world unit.
  static const float TILE_SIZE =
      1.0f; // Target tile size in world units (1 grid square)
  static const float MIN_TILE =
      0.3f; // Min remainder that gets its own tile (0.3 tile units)
  static const float MIN_WIDTH =
      0.3f; // Min width of non-split dimension to allow subdivision

  std::set<std::tuple<std::string, int, int>> modifiedMeshes;
  for (const auto &sf : vp.m_selectedFaces)
    modifiedMeshes.insert({sf.chunkName, sf.objectIdx, sf.meshIdx});

  int facesSubdivided = 0;

  for (const auto &mRef : modifiedMeshes) {
    const std::string &cName = std::get<0>(mRef);
    int oIdx = std::get<1>(mRef);
    int mIdx = std::get<2>(mRef);

    for (const auto &lcConst : vp.GetChunks()) {
      auto &lc = const_cast<LoadedChunk &>(lcConst);
      if (lc.data->chunkName != cName || oIdx >= (int)lc.data->objects.size())
        continue;
      auto &obj = lc.data->objects[oIdx];
      if (mIdx >= (int)obj.meshes.size())
        break;
      auto &mesh = obj.meshes[mIdx];

      MeshSnapshot snap;
      if (vp.m_history) {
        snap.chunkName = cName;
        snap.objectIdx = oIdx;
        snap.meshIdx = mIdx;
        snap.before = mesh;
        snap.description = "Subdivide Faces";
      }

      std::set<int> selectedFaceIndices;
      for (const auto &sf : vp.m_selectedFaces)
        if (sf.chunkName == cName && sf.objectIdx == oIdx && sf.meshIdx == mIdx)
          selectedFaceIndices.insert(sf.faceIdx);

      std::vector<RenderFace> newFaces;

      for (int fIdx = 0; fIdx < (int)mesh.faces.size(); ++fIdx) {
        const RenderFace &face = mesh.faces[fIdx];

        if (selectedFaceIndices.find(fIdx) == selectedFaceIndices.end()) {
          newFaces.push_back(face);
          continue;
        }

        // Triangles – skip silently
        if (face.v[3] == 0xFF) {
          newFaces.push_back(face);
          continue;
        }

        // v0 (TL), v1 (BL), v2 (BR), v3 (TR) in RenderFace winding order
        Vector3 v0 = {mesh.vx[face.v[0]], mesh.vy[face.v[0]], mesh.vz[face.v[0]]};
        Vector3 v1 = {mesh.vx[face.v[1]], mesh.vy[face.v[1]], mesh.vz[face.v[1]]};
        Vector3 v2 = {mesh.vx[face.v[2]], mesh.vy[face.v[2]], mesh.vz[face.v[2]]};
        Vector3 v3 = {mesh.vx[face.v[3]], mesh.vy[face.v[3]], mesh.vz[face.v[3]]};

        // Edge lengths — in RenderFace winding:
        // Edge 01 (v0->v1) and Edge 23 (v2->v3) are vertical height edges
        // Edge 12 (v1->v2) and Edge 30 (v3->v0) are horizontal width edges
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

        // Vertex budget check for PS1 PLM 255-vertex limit
        int maxNewVerts = (N - 1) * 2;
        if (mesh.vx.size() + maxNewVerts > 255) {
          printf("[Subdivide] WARNING: Face %d skipped! Mesh '%s' vertex count "
                 "(%zu + %d) would exceed PS1 max limit of 255 vertices.\n",
                 fIdx, obj.name.c_str(), mesh.vx.size(), maxNewVerts);
          newFaces.push_back(face);
          continue;
        }

        std::vector<float> cutT;
        for (int i = 1; i < N; i++)
          cutT.push_back((i * TILE_SIZE) / L);

        auto lerp3 = [](Vector3 a, Vector3 b, float t) {
          return Vector3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
        };
        auto lerpf = [](float a, float b, float t) { return a + (b - a) * t; };

        Vector3 eA0, eA1, eB0, eB1;
        float uvA0u, uvA0v, uvA1u, uvA1v;
        float uvB0u, uvB0v, uvB1u, uvB1v;
        float rawUA0, rawVA0, rawUA1, rawVA1;
        float rawUB0, rawVB0, rawUB1, rawVB1;

        if (splitVert) {
          eA0 = v0; eA1 = v1; eB0 = v3; eB1 = v2;
          uvA0u = face.uv[0][0]; uvA0v = face.uv[0][1];
          uvA1u = face.uv[1][0]; uvA1v = face.uv[1][1];
          uvB0u = face.uv[3][0]; uvB0v = face.uv[3][1];
          uvB1u = face.uv[2][0]; uvB1v = face.uv[2][1];
          rawUA0 = face.rawU[0]; rawVA0 = face.rawV[0];
          rawUA1 = face.rawU[1]; rawVA1 = face.rawV[1];
          rawUB0 = face.rawU[3]; rawVB0 = face.rawV[3];
          rawUB1 = face.rawU[2]; rawVB1 = face.rawV[2];
        } else {
          eA0 = v0; eA1 = v3; eB0 = v1; eB1 = v2;
          uvA0u = face.uv[0][0]; uvA0v = face.uv[0][1];
          uvA1u = face.uv[3][0]; uvA1v = face.uv[3][1];
          uvB0u = face.uv[1][0]; uvB0v = face.uv[1][1];
          uvB1u = face.uv[2][0]; uvB1v = face.uv[2][1];
          rawUA0 = face.rawU[0]; rawVA0 = face.rawV[0];
          rawUA1 = face.rawU[3]; rawVA1 = face.rawV[3];
          rawUB0 = face.rawU[1]; rawVB0 = face.rawV[1];
          rawUB1 = face.rawU[2]; rawVB1 = face.rawV[2];
        }

        std::vector<float> ts;
        ts.push_back(0.0f);
        for (float t : cutT) ts.push_back(t);
        ts.push_back(1.0f);

        size_t initialVxCount = mesh.vx.size();
        bool capacityExceeded = false;

        auto getOrCreate = [&](Vector3 pos) -> uint8_t {
          for (size_t k = 0; k < mesh.vx.size(); ++k) {
            if (fabsf(mesh.vx[k] - pos.x) < 0.001f &&
                fabsf(mesh.vy[k] - pos.y) < 0.001f &&
                fabsf(mesh.vz[k] - pos.z) < 0.001f)
              return (uint8_t)std::min(k, (size_t)254);
          }
          if (mesh.vx.size() >= 255) {
            capacityExceeded = true;
            return 254;
          }
          mesh.vx.push_back(pos.x);
          mesh.vy.push_back(pos.y);
          mesh.vz.push_back(pos.z);
          return (uint8_t)(mesh.vx.size() - 1);
        };

        std::vector<RenderFace> emittedQuads;

        for (int seg = 0; seg < N; ++seg) {
          float t0 = ts[seg], t1 = ts[seg + 1];

          Vector3 pA0 = lerp3(eA0, eA1, t0);
          Vector3 pA1 = lerp3(eA0, eA1, t1);
          Vector3 pB0 = lerp3(eB0, eB1, t0);
          Vector3 pB1 = lerp3(eB0, eB1, t1);

          float uA0 = lerpf(uvA0u, uvA1u, t0), vA0 = lerpf(uvA0v, uvA1v, t0);
          float uA1 = lerpf(uvA0u, uvA1u, t1), vA1 = lerpf(uvA0v, uvA1v, t1);
          float uB0 = lerpf(uvB0u, uvB1u, t0), vB0 = lerpf(uvB0v, uvB1v, t0);
          float uB1 = lerpf(uvB0u, uvB1u, t1), vB1 = lerpf(uvB0v, uvB1v, t1);

          float rUA0 = lerpf(rawUA0, rawUA1, t0), rVA0 = lerpf(rawVA0, rawVA1, t0);
          float rUA1 = lerpf(rawUA0, rawUA1, t1), rVA1 = lerpf(rawVA0, rawVA1, t1);
          float rUB0 = lerpf(rawUB0, rawUB1, t0), rVB0 = lerpf(rawVB0, rawVB1, t0);
          float rUB1 = lerpf(rawUB0, rawUB1, t1), rVB1 = lerpf(rawVB0, rawVB1, t1);

          RenderFace sub = face;
          if (splitVert) {
            sub.v[0] = getOrCreate(pA0);
            sub.uv[0][0] = uA0; sub.uv[0][1] = vA0;
            sub.rawU[0] = (uint8_t)std::clamp((int)lroundf(rUA0), 0, 255);
            sub.rawV[0] = (uint8_t)std::clamp((int)lroundf(rVA0), 0, 255);

            sub.v[1] = getOrCreate(pA1);
            sub.uv[1][0] = uA1; sub.uv[1][1] = vA1;
            sub.rawU[1] = (uint8_t)std::clamp((int)lroundf(rUA1), 0, 255);
            sub.rawV[1] = (uint8_t)std::clamp((int)lroundf(rVA1), 0, 255);

            sub.v[2] = getOrCreate(pB1);
            sub.uv[2][0] = uB1; sub.uv[2][1] = vB1;
            sub.rawU[2] = (uint8_t)std::clamp((int)lroundf(rUB1), 0, 255);
            sub.rawV[2] = (uint8_t)std::clamp((int)lroundf(rVB1), 0, 255);

            sub.v[3] = getOrCreate(pB0);
            sub.uv[3][0] = uB0; sub.uv[3][1] = vB0;
            sub.rawU[3] = (uint8_t)std::clamp((int)lroundf(rUB0), 0, 255);
            sub.rawV[3] = (uint8_t)std::clamp((int)lroundf(rVB0), 0, 255);
          } else {
            sub.v[0] = getOrCreate(pA0);
            sub.uv[0][0] = uA0; sub.uv[0][1] = vA0;
            sub.rawU[0] = (uint8_t)std::clamp((int)lroundf(rUA0), 0, 255);
            sub.rawV[0] = (uint8_t)std::clamp((int)lroundf(rVA0), 0, 255);

            sub.v[1] = getOrCreate(pB0);
            sub.uv[1][0] = uB0; sub.uv[1][1] = vB0;
            sub.rawU[1] = (uint8_t)std::clamp((int)lroundf(rUB0), 0, 255);
            sub.rawV[1] = (uint8_t)std::clamp((int)lroundf(rVB0), 0, 255);

            sub.v[2] = getOrCreate(pB1);
            sub.uv[2][0] = uB1; sub.uv[2][1] = vB1;
            sub.rawU[2] = (uint8_t)std::clamp((int)lroundf(rUB1), 0, 255);
            sub.rawV[2] = (uint8_t)std::clamp((int)lroundf(rVB1), 0, 255);

            sub.v[3] = getOrCreate(pA1);
            sub.uv[3][0] = uA1; sub.uv[3][1] = vA1;
            sub.rawU[3] = (uint8_t)std::clamp((int)lroundf(rUA1), 0, 255);
            sub.rawV[3] = (uint8_t)std::clamp((int)lroundf(rVA1), 0, 255);
          }

          if (capacityExceeded)
            break;

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

        for (const auto &q : emittedQuads)
          newFaces.push_back(q);

        facesSubdivided++;
      }

      mesh.faces = newFaces;

      if (facesSubdivided > 0) {
        if (vp.m_history) {
          snap.after = mesh;
          vp.m_history->Push(std::move(snap));
        }
        std::string ws = vp.m_lastWorkspaceDir;
        if (std::filesystem::exists("data/workspace")) ws = "data/workspace";
        else if (std::filesystem::exists("../data/workspace")) ws = "../data/workspace";
        vp.RebuildChunkBatches(cName, ws);
      }

      break;
    }
  }

  if (facesSubdivided > 0) {
    printf("[Geometry] Subdivided %d faces\n", facesSubdivided);
    vp.m_selectedFaces.clear();
  } else {
    printf("[Geometry] Subdivide: no eligible quads found (>1.0 world units on "
           "longest edge, width>=0.3 units, verts<=255)\n");
  }
}

} // namespace Geometry
