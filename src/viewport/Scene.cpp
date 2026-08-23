#include "viewport/Scene.h"
#include "formats/IPDParse.h"
#include "core/Textures.h"
#include "raymath.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cfloat>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SceneOverlay::SceneOverlay() : ViewportBase("Scene") {}

SceneOverlay::~SceneOverlay() {
  UnloadAll(); // Triggers OnUnloadAll() in base class via shutdown, but safe to
               // call here too
}

void SceneOverlay::OnUnloadAll() {
  for (auto &c : m_chunks) {
    FreeGpuBatches(c);
  }
  m_chunks.clear();
}

// ---------------------------------------------------------------------------
// Chunk management
// ---------------------------------------------------------------------------

bool SceneOverlay::LoadChunk(std::shared_ptr<ParsedChunk> parsedChunk,
                             const std::string &workspaceDir) {
  if (!parsedChunk)
    return false;

  m_lastWorkspaceDir = workspaceDir;
  // If we already have it loaded, remove it first
  UnloadChunk(parsedChunk->chunkName);

  LoadedChunk newChunk;
  newChunk.data = parsedChunk;
  newChunk.visible = true;
  newChunk.bounds = {{99999.0f, 99999.0f, 99999.0f},
                     {-99999.0f, -99999.0f, -99999.0f}};

  if (!newChunk.hasError) {
    // BuildBatches was extracted into IPDParse
    BuildGpuBatches(newChunk, workspaceDir);
  }

  m_chunks.push_back(std::move(newChunk));
  return true;
}

void SceneOverlay::UnloadChunk(const std::string &chunkName) {
  for (size_t i = 0; i < m_chunks.size(); ++i) {
    if (m_chunks[i].data->chunkName == chunkName) {
      FreeGpuBatches(m_chunks[i]);
      // O(1) removal using swap-and-pop instead of O(N) element shifting
      std::swap(m_chunks[i], m_chunks.back());
      m_chunks.pop_back();
      return;
    }
  }
}

// ---------------------------------------------------------------------------
// GPU batch building
// ---------------------------------------------------------------------------

void SceneOverlay::BuildGpuBatches(LoadedChunk &lc,
                                   const std::string &workspaceDir) {
  lc.batches.clear();
  TextureCache &cache = TextureCache::Get();

  for (const auto &batch : lc.data->batches) {
    if (batch.vertexCount == 0)
      continue;

    // Resolve texture name
    std::string texName = batch.texName;
    bool isNoTex = texName.empty();

    GpuBatch gpuBatch;
    gpuBatch.paletteRow = batch.paletteRow;
    gpuBatch.texName = texName;

    // --- Build Raylib Mesh ---
    Mesh mesh = {0};
    mesh.vertexCount = batch.vertexCount;
    mesh.triangleCount = batch.vertexCount / 3;

    // Raylib expects heap-allocated arrays that it will free via UnloadMesh
    mesh.vertices = (float *)RL_MALLOC(batch.positions.size() * sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(batch.texcoords.size() * sizeof(float));
    memcpy(mesh.vertices, batch.positions.data(),
           batch.positions.size() * sizeof(float));
    memcpy(mesh.texcoords, batch.texcoords.data(),
           batch.texcoords.size() * sizeof(float));

    UploadMesh(&mesh, true); // true = dynamic draw
    gpuBatch.mesh = mesh;
    gpuBatch.meshUploaded = true;

    BoundingBox meshBox = GetMeshBoundingBox(mesh);
    lc.bounds.min.x = std::min(lc.bounds.min.x, meshBox.min.x);
    lc.bounds.min.y = std::min(lc.bounds.min.y, meshBox.min.y);
    lc.bounds.min.z = std::min(lc.bounds.min.z, meshBox.min.z);
    lc.bounds.max.x = std::max(lc.bounds.max.x, meshBox.max.x);
    lc.bounds.max.y = std::max(lc.bounds.max.y, meshBox.max.y);
    lc.bounds.max.z = std::max(lc.bounds.max.z, meshBox.max.z);

    // --- Build Raylib Material ---
    gpuBatch.material = LoadMaterialDefault();
    if (!texName.empty() && !isNoTex) {
      Texture2D tex = cache.Fetch(texName, batch.paletteRow, workspaceDir);
      if (tex.id != 0) {
        gpuBatch.material.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
      }
    }

    lc.batches.push_back(std::move(gpuBatch));
  }

  printf("[SceneOverlay] Built %zu GPU batches for '%s'\n", lc.batches.size(),
         lc.data->chunkName.c_str());
}

void SceneOverlay::FreeGpuBatches(LoadedChunk &lc) { lc.batches.clear(); }

// ---------------------------------------------------------------------------
// DrawScene — called by ViewportBase between BeginMode3D/EndMode3D
// ---------------------------------------------------------------------------

void SceneOverlay::DrawScene() {
  // Draw all visible chunks
  static const Matrix identity = MatrixIdentity();
  for (const auto &lc : m_chunks) {
    if (!lc.visible || lc.hasError)
      continue;
    for (const auto &b : lc.batches) {
      if (!b.meshUploaded)
        continue;
      DrawMesh(b.mesh, b.material, identity);
    }

    if (lc.data->chunkName == m_selectedChunk && m_selectedObjectIdx >= 0 &&
        m_selectedObjectIdx < (int)lc.data->objects.size()) {
      const auto &obj = lc.data->objects[m_selectedObjectIdx];

      rlDisableDepthTest();
      DrawBoundingBox(obj.bounds, YELLOW);
      rlEnableDepthTest();
    }

    // Vertex rendering removed
  }
}

std::vector<ChunkLocation> SceneOverlay::GetChunkLocations() const {
  std::vector<ChunkLocation> locs;
  locs.reserve(m_chunks.size());
  for (const auto &lc : m_chunks) {
    locs.push_back(
        {lc.data->chunkName, lc.data->xPos, lc.data->yPos, lc.bounds});
  }
  return locs;
}

void SceneOverlay::RebuildChunkBatches(const std::string &chunkName,
                                       const std::string &workspaceDir) {
  for (auto &chunk : m_chunks) {
    if (chunk.data->chunkName == chunkName) {
      std::vector<GpuBatch> oldBatches = std::move(chunk.batches);
      IPDParse::BuildBatches(*chunk.data);

      bool canFastUpdate = (oldBatches.size() == chunk.data->batches.size());
      if (canFastUpdate) {
        for (size_t i = 0; i < oldBatches.size(); i++) {
          if (oldBatches[i].mesh.vertexCount !=
              chunk.data->batches[i].vertexCount) {
            canFastUpdate = false;
            break;
          }
        }
      }

      if (canFastUpdate) {
        chunk.bounds = {{99999.0f, 99999.0f, 99999.0f},
                        {-99999.0f, -99999.0f, -99999.0f}};

        for (size_t i = 0; i < oldBatches.size(); i++) {
          auto &b = oldBatches[i];
          const auto &rb = chunk.data->batches[i];

          memcpy(b.mesh.vertices, rb.positions.data(),
                 rb.positions.size() * sizeof(float));
          memcpy(b.mesh.texcoords, rb.texcoords.data(),
                 rb.texcoords.size() * sizeof(float));

          UpdateMeshBuffer(b.mesh, 0, b.mesh.vertices,
                           rb.positions.size() * sizeof(float), 0);
          UpdateMeshBuffer(b.mesh, 1, b.mesh.texcoords,
                           rb.texcoords.size() * sizeof(float), 0);

          BoundingBox meshBox = GetMeshBoundingBox(b.mesh);
          chunk.bounds.min.x = std::min(chunk.bounds.min.x, meshBox.min.x);
          chunk.bounds.min.y = std::min(chunk.bounds.min.y, meshBox.min.y);
          chunk.bounds.min.z = std::min(chunk.bounds.min.z, meshBox.min.z);
          chunk.bounds.max.x = std::max(chunk.bounds.max.x, meshBox.max.x);
          chunk.bounds.max.y = std::max(chunk.bounds.max.y, meshBox.max.y);
          chunk.bounds.max.z = std::max(chunk.bounds.max.z, meshBox.max.z);
        }

        chunk.batches = std::move(oldBatches);
      } else {
        chunk.batches = std::move(oldBatches);
        FreeGpuBatches(chunk);
        BuildGpuBatches(chunk, workspaceDir);
      }
      break;
    }
  }
}

void SceneOverlay::HandlePicking(Ray ray) {
  float closestDist = FLT_MAX;
  std::string hitChunk;
  int hitObjIdx = -1;

  for (const auto &lc : m_chunks) {
    if (!lc.visible || lc.hasError)
      continue;

    for (size_t i = 0; i < lc.data->objects.size(); ++i) {
      const auto &obj = lc.data->objects[i];

      // Skip invalid bounds
      if (obj.bounds.min.x > obj.bounds.max.x)
        continue;

      // Ray-AABB test
      bool insideBox = (m_camera.position.x >= obj.bounds.min.x &&
                        m_camera.position.x <= obj.bounds.max.x &&
                        m_camera.position.y >= obj.bounds.min.y &&
                        m_camera.position.y <= obj.bounds.max.y &&
                        m_camera.position.z >= obj.bounds.min.z &&
                        m_camera.position.z <= obj.bounds.max.z);

      if (!insideBox) {
        RayCollision boxHit = GetRayCollisionBox(ray, obj.bounds);
        if (!boxHit.hit)
          continue;
      }

      // Ray-Triangle precise test
      for (size_t mi = 0; mi < obj.meshes.size(); ++mi) {
        const auto &mesh = obj.meshes[mi];
        for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
          const auto &face = mesh.faces[fi];
          bool isQuad = (face.v[3] != 0xFF);
          int triCount = isQuad ? 2 : 1;
          static const int triV[2][3] = {{0, 1, 2}, {0, 2, 3}};

          for (int t = 0; t < triCount; ++t) {
            Vector3 v1 = {mesh.vx[face.v[triV[t][0]]],
                          mesh.vy[face.v[triV[t][0]]],
                          mesh.vz[face.v[triV[t][0]]]};
            Vector3 v2 = {mesh.vx[face.v[triV[t][1]]],
                          mesh.vy[face.v[triV[t][1]]],
                          mesh.vz[face.v[triV[t][1]]]};
            Vector3 v3 = {mesh.vx[face.v[triV[t][2]]],
                          mesh.vy[face.v[triV[t][2]]],
                          mesh.vz[face.v[triV[t][2]]]};

            RayCollision triHit = GetRayCollisionTriangle(ray, v1, v2, v3);
            if (triHit.hit && triHit.distance < closestDist) {
              closestDist = triHit.distance;
              hitChunk = lc.data->chunkName;
              hitObjIdx = (int)i;
            }
          }
        }
      }
    }
  }

  m_selectedChunk = hitChunk;
  m_selectedObjectIdx = hitObjIdx;
}
