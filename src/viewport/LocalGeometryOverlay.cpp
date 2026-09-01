#include "viewport/LocalGeometryOverlay.h"
#include "core/Config.h"
#include "core/History.h"
#include "formats/IPDParse.h"
#include "core/Textures.h"
#include "panels/TextureMapPanel.h"
#include "viewport/Viewport.h"
#include "imgui.h"
#include "raymath.h"
#include "rlgl.h"
#include "viewport/Frustum.h"
#include "viewport/Wireframe.h"
#include "geometry/MeshOperations.h"
#include "geometry/GlobalObjectOperations.h"
#include "geometry/SubdivideFace.h"
#include "geometry/FaceOperations.h"
#include "geometry/VertexOperations.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static Vector3 GetDominantAxis(Vector3 v) {
  Vector3 absV = {fabsf(v.x), fabsf(v.y), fabsf(v.z)};
  if (absV.x > absV.y && absV.x > absV.z)
    return {v.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f};
  if (absV.y > absV.x && absV.y > absV.z)
    return {0.0f, v.y > 0 ? 1.0f : -1.0f, 0.0f};
  return {0.0f, 0.0f, v.z > 0 ? 1.0f : -1.0f};
}

// Check if 2D line segment (p1, p2) intersects with AABB rectangle
static inline bool SegmentIntersectsBox2D(Vector2 p1, Vector2 p2, Rectangle box) {
  float bx0 = box.x;
  float bx1 = box.x + box.width;
  float by0 = box.y;
  float by1 = box.y + box.height;

  // Quick bounding box rejection
  float segMinX = std::min(p1.x, p2.x);
  float segMaxX = std::max(p1.x, p2.x);
  float segMinY = std::min(p1.y, p2.y);
  float segMaxY = std::max(p1.y, p2.y);

  if (segMaxX < bx0 || segMinX > bx1 || segMaxY < by0 || segMinY > by1)
    return false;

  // If either endpoint is inside the box
  if ((p1.x >= bx0 && p1.x <= bx1 && p1.y >= by0 && p1.y <= by1) ||
      (p2.x >= bx0 && p2.x <= bx1 && p2.y >= by0 && p2.y <= by1))
    return true;

  // Parametric intersection with the 4 box edge lines
  float dx = p2.x - p1.x;
  float dy = p2.y - p1.y;

  if (fabsf(dx) > 1e-6f) {
    float t = (bx0 - p1.x) / dx;
    if (t >= 0.0f && t <= 1.0f) {
      float y = p1.y + t * dy;
      if (y >= by0 && y <= by1) return true;
    }
    t = (bx1 - p1.x) / dx;
    if (t >= 0.0f && t <= 1.0f) {
      float y = p1.y + t * dy;
      if (y >= by0 && y <= by1) return true;
    }
  }

  if (fabsf(dy) > 1e-6f) {
    float t = (by0 - p1.y) / dy;
    if (t >= 0.0f && t <= 1.0f) {
      float x = p1.x + t * dx;
      if (x >= bx0 && x <= bx1) return true;
    }
    t = (by1 - p1.y) / dy;
    if (t >= 0.0f && t <= 1.0f) {
      float x = p1.x + t * dx;
      if (x >= bx0 && x <= bx1) return true;
    }
  }

  return false;
}

// Check if 2D point is inside a triangle
static inline bool PointInTriangle2D(Vector2 p, Vector2 a, Vector2 b, Vector2 c) {
  float cross1 = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
  float cross2 = (c.x - b.x) * (p.y - b.y) - (c.y - b.y) * (p.x - b.x);
  float cross3 = (a.x - c.x) * (p.y - c.y) - (a.y - c.y) * (p.x - c.x);

  bool hasNeg = (cross1 < 0.0f) || (cross2 < 0.0f) || (cross3 < 0.0f);
  bool hasPos = (cross1 > 0.0f) || (cross2 > 0.0f) || (cross3 > 0.0f);

  return !(hasNeg && hasPos);
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

LocalGeometryOverlay::LocalGeometryOverlay() {}

LocalGeometryOverlay::~LocalGeometryOverlay() {
  UnloadAll();
}

void LocalGeometryOverlay::UnloadAll() {
  if (!m_sharedChunks) {
    for (auto &c : m_chunks) {
      FreeGpuBatches(c);
    }
    m_chunks.clear();
  }
}

// ---------------------------------------------------------------------------
// Chunk management
// ---------------------------------------------------------------------------

std::vector<std::string> LocalGeometryOverlay::ConsumeModifiedChunks() {
  std::vector<std::string> res = m_modifiedChunks;
  m_modifiedChunks.clear();
  return res;
}

bool LocalGeometryOverlay::LoadChunk(std::shared_ptr<ParsedChunk> parsedChunk,
                                      const std::string &workspaceDir) {
  if (!parsedChunk)
    return false;

  UnloadChunk(parsedChunk->chunkName);

  LoadedChunk lc;
  lc.data = parsedChunk;
  lc.visible = true;
  lc.hasError = false;
  lc.bounds = {{99999.0f, 99999.0f, 99999.0f},
               {-99999.0f, -99999.0f, -99999.0f}};

  BuildGpuBatches(lc, workspaceDir);

  m_chunks.push_back(std::move(lc));
  return true;
}

void LocalGeometryOverlay::UnloadChunk(const std::string &chunkName) {
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

void LocalGeometryOverlay::BuildGpuBatches(LoadedChunk &lc,
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
    gpuBatch.material = cache.CreateMeshMaterial(texName, batch.paletteRow, workspaceDir);

    lc.batches.push_back(std::move(gpuBatch));
  }

  printf("[LocalGeometryOverlay] Built %zu GPU batches for '%s'\n",
         lc.batches.size(), lc.data->chunkName.c_str());
}

void LocalGeometryOverlay::FreeGpuBatches(LoadedChunk &lc) {
  lc.batches.clear();
}

void LocalGeometryOverlay::DrawOverlay(Viewport &vp) {
  // Extract camera frustum for hierarchical culling
  Frustum frustum =
      Frustum::FromCamera(vp.GetCamera(), (float)vp.GetWidth() / (float)vp.GetHeight());

  bool hasSelection = false;
  if (m_editMode == EditMode::Vertex && !m_selectedVertices.empty()) hasSelection = true;
  else if (m_editMode == EditMode::Face && !m_selectedFaces.empty()) hasSelection = true;
  else if ((m_editMode == EditMode::Mesh || m_editMode == EditMode::GlobalObject) && !m_selectedChunk.empty() && m_selectedObjectIdx >= 0) hasSelection = true;

  // --- Input Handling ---
  if (vp.IsHovered() && !GetChunks().empty() && hasSelection) {
      Vector3 camForward = Vector3Normalize(Vector3Subtract(vp.GetCamera().target, vp.GetCamera().position));
      Vector3 camUp = vp.GetCamera().up;
      Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camForward, camUp));

      Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
      if (IsKeyPressed(KEY_UP)) {
          moveDir = GetDominantAxis({camForward.x, 0.0f, camForward.z});
      }
      if (IsKeyPressed(KEY_DOWN)) {
          Vector3 d = GetDominantAxis({camForward.x, 0.0f, camForward.z});
          moveDir = {-d.x, -d.y, -d.z};
      }
      if (IsKeyPressed(KEY_LEFT)) {
          Vector3 d = GetDominantAxis({camRight.x, 0.0f, camRight.z});
          moveDir = {-d.x, -d.y, -d.z};
      }
      if (IsKeyPressed(KEY_RIGHT)) {
          moveDir = GetDominantAxis({camRight.x, 0.0f, camRight.z});
      }
      if (IsKeyPressed(KEY_PAGE_UP)) {
          moveDir = {0.0f, 1.0f, 0.0f};
      }
      if (IsKeyPressed(KEY_PAGE_DOWN)) {
          moveDir = {0.0f, -1.0f, 0.0f};
      }

      if (moveDir.x != 0.0f || moveDir.y != 0.0f || moveDir.z != 0.0f) {
          float step = IPD_SCALE * (float)(1 << m_moveStepPower) * (IsKeyDown(KEY_LEFT_SHIFT) ? 16.0f : 1.0f);
          Vector3 delta = { moveDir.x * step, moveDir.y * step, moveDir.z * step };
          TranslateSelection(delta);
      }
  }

  // --- Draw all visible chunks ---
  static const Matrix identity = MatrixIdentity();
  for (const auto &lc : GetChunks()) {
    if (!lc.visible || lc.hasError)
      continue;
    bool isChunkInFrustum = frustum.IsBoxInFrustum(lc.bounds);

    if (isChunkInFrustum) {
      for (const auto &b : lc.batches) {
        if (!b.meshUploaded)
          continue;
        DrawMesh(b.mesh, b.material, identity);
      }
    }

    // --- Mesh & Global Object mode: draw global object origin cubes and selected AABB ---
    if (m_editMode == EditMode::Mesh || m_editMode == EditMode::GlobalObject) {
      for (size_t i = 0; i < lc.data->objects.size(); i++) {
        const auto &obj = lc.data->objects[i];
        if (obj.isGlobal) {
          Vector3 origin = {
              ((float)obj.rawTx + 10240.0f * (float)lc.data->xPos) *
                  (1.0f / 256.0f),
              -((float)obj.rawTy) * (1.0f / 256.0f),
              -((float)obj.rawTz + 10240.0f * (float)lc.data->yPos) *
                  (1.0f / 256.0f)};
          DrawCube(origin, 0.2f, 0.2f, 0.2f, {0, 255, 255, 255});
        }
      }
    }

    if (lc.data->chunkName == m_selectedChunk && m_selectedObjectIdx >= 0 &&
        m_selectedObjectIdx < (int)lc.data->objects.size()) {
      const auto &obj = lc.data->objects[m_selectedObjectIdx];

      if (m_editMode == EditMode::Mesh || m_editMode == EditMode::GlobalObject) {
        rlDisableDepthTest();
        rlDrawRenderBatchActive();
        rlSetLineWidth(Config::Get().WireframeThickness);
        DrawBoundingBox(obj.bounds,
                        obj.isGlobal ? Color{0, 255, 255, 255} : YELLOW);
        if (obj.isGlobal) {
          Vector3 origin = {
              ((float)obj.rawTx + 10240.0f * (float)lc.data->xPos) *
                  (1.0f / 256.0f),
              -((float)obj.rawTy) * (1.0f / 256.0f),
              -((float)obj.rawTz + 10240.0f * (float)lc.data->yPos) *
                  (1.0f / 256.0f)};
          DrawCubeWires(origin, 0.5f, 0.5f, 0.5f, {0, 255, 255, 255});
        }
        rlDrawRenderBatchActive();
        rlSetLineWidth(1.0f);
        rlEnableDepthTest();
      }
    }
  }

  // --- Wireframe overlays ---
  rlDisableDepthTest();
  rlDrawRenderBatchActive();
  rlSetLineWidth(Config::Get().WireframeThickness);

  if (m_editMode == EditMode::Face || m_editMode == EditMode::Vertex) {
      struct MeshID {
          std::string chunkName;
          int objIdx;
          int meshIdx;
          bool operator<(const MeshID& o) const {
              if (chunkName != o.chunkName) return chunkName < o.chunkName;
              if (objIdx != o.objIdx) return objIdx < o.objIdx;
              return meshIdx < o.meshIdx;
          }
      };
      
      std::set<MeshID> activeMeshes;
      
      if (m_editMode == EditMode::Face) {
          for (const auto& f : m_selectedFaces) {
              activeMeshes.insert(MeshID{f.chunkName, f.objectIdx, f.meshIdx});
          }
      } else {
          for (const auto& v : m_selectedVertices) {
              activeMeshes.insert(MeshID{v.chunkName, v.objectIdx, v.meshIdx});
          }
      }
      
      // If no faces/vertices are selected but we have a single active mesh from UI/click
      if (activeMeshes.empty() && !m_selectedChunk.empty() && m_selectedObjectIdx >= 0 && m_selectedMeshIdx >= 0) {
          activeMeshes.insert(MeshID{m_selectedChunk, m_selectedObjectIdx, m_selectedMeshIdx});
      }

      for (const auto& lc : GetChunks()) {
          if (!lc.visible || lc.hasError) continue;
          for (const auto& mid : activeMeshes) {
              if (mid.chunkName == lc.data->chunkName) {
                  if (mid.objIdx >= 0 && mid.objIdx < (int)lc.data->objects.size()) {
                      const auto& obj = lc.data->objects[mid.objIdx];
                      if (mid.meshIdx >= 0 && mid.meshIdx < (int)obj.meshes.size()) {
                          Wireframe::DrawMeshWireframe(obj.meshes[mid.meshIdx], Config::Get().WireframeColor, Config::Get().WireframeThickness);
                      }
                  }
              }
          }
      }
  } else {
      // Object mode
      if (!m_selectedChunk.empty() && m_selectedObjectIdx >= 0) {
          for (const auto& lc : GetChunks()) {
              if (lc.data->chunkName == m_selectedChunk && lc.visible && !lc.hasError) {
                  if (m_selectedObjectIdx < (int)lc.data->objects.size()) {
                      const auto& obj = lc.data->objects[m_selectedObjectIdx];
                      if (m_selectedMeshIdx >= 0 && m_selectedMeshIdx < (int)obj.meshes.size()) {
                          Wireframe::DrawMeshWireframe(obj.meshes[m_selectedMeshIdx], Config::Get().WireframeColor, Config::Get().WireframeThickness);
                      } else {
                          Wireframe::DrawObjectWireframe(obj, Config::Get().WireframeColor, Config::Get().WireframeThickness);
                      }
                  }
                  break;
              }
          }
      }
  }

  rlDrawRenderBatchActive();
  rlSetLineWidth(1.0f);
  rlEnableDepthTest();

  if (m_editMode == EditMode::Face) {
    Wireframe::DrawSelectedFaceOutlines(m_selectedFaces, GetChunks());
  }

  if (m_editMode == EditMode::Vertex) {
    Wireframe::DrawVertexOverlay(
        GetChunks(), m_selectedVertices, vp.IsHovered(),
        vp.GetCamera(), vp.GetWidth(), vp.GetHeight(), vp.GetLocalMousePos(), frustum);
  }

  if (m_editMode == EditMode::Vertex) {
    rlDisableDepthTest();
    for (const auto &sv : m_selectedVertices) {
      for (const auto &lc : GetChunks()) {
        if (lc.data->chunkName == sv.chunkName) {
          if (sv.objectIdx < (int)lc.data->objects.size()) {
            const auto &obj = lc.data->objects[sv.objectIdx];
            if (sv.meshIdx < (int)obj.meshes.size()) {
              const auto &mesh = obj.meshes[sv.meshIdx];
              if (sv.vertexIdx < (int)mesh.vx.size()) {
                Vector3 v = {mesh.vx[sv.vertexIdx], mesh.vy[sv.vertexIdx],
                             mesh.vz[sv.vertexIdx]};
                if (frustum.IsPointInFrustum(v)) {
                  DrawCube(v, 0.05f, 0.05f, 0.05f, GREEN);
                }
              }
            }
          }
          break;
        }
      }
    }
    rlEnableDepthTest();
  }
  
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_B) &&
      m_editMode == EditMode::Face) {
    HandleTilePainting(vp, GetScreenToWorldRay(vp.GetLocalMousePos(), vp.GetCamera()));
  }
}

std::vector<ChunkLocation> LocalGeometryOverlay::GetChunkLocations() const {
  std::vector<ChunkLocation> locs;
  locs.reserve(GetChunks().size());
  for (const auto &lc : GetChunks()) {
    locs.push_back(
        {lc.data->chunkName, lc.data->xPos, lc.data->yPos, lc.bounds});
  }
  return locs;
}

void LocalGeometryOverlay::RebuildChunkBatches(
    const std::string &chunkName, const std::string &workspaceDir) {
  if (std::find(m_modifiedChunks.begin(), m_modifiedChunks.end(), chunkName) ==
      m_modifiedChunks.end()) {
    m_modifiedChunks.push_back(chunkName);
  }
  for (const auto &chunkConst : GetChunks()) {
    auto &chunk = const_cast<LoadedChunk &>(chunkConst);
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

          b.texName = rb.texName;
          b.paletteRow = rb.paletteRow;
          if (b.material.maps != nullptr) {
            b.material.maps[MATERIAL_MAP_DIFFUSE].texture =
                TextureCache::Get().Fetch(b.texName, b.paletteRow, workspaceDir);
          }

          bool changedPos = memcmp(b.mesh.vertices, rb.positions.data(),
                                   rb.positions.size() * sizeof(float)) != 0;
          bool changedTex = memcmp(b.mesh.texcoords, rb.texcoords.data(),
                                   rb.texcoords.size() * sizeof(float)) != 0;

          if (changedPos) {
            memcpy(b.mesh.vertices, rb.positions.data(),
                   rb.positions.size() * sizeof(float));
            UpdateMeshBuffer(b.mesh, 0, b.mesh.vertices,
                             rb.positions.size() * sizeof(float), 0);
          }
          if (changedTex) {
            memcpy(b.mesh.texcoords, rb.texcoords.data(),
                   rb.texcoords.size() * sizeof(float));
            UpdateMeshBuffer(b.mesh, 1, b.mesh.texcoords,
                             rb.texcoords.size() * sizeof(float), 0);
          }

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

void LocalGeometryOverlay::RebuildChunksUsingTexture(
    const std::string &texName, const std::string &workspaceDir,
    Viewport &sceneViewport) {
  if (texName.empty())
    return;

  for (const auto &lc : GetChunks()) {
    if (lc.data) {
      bool usesTexture = false;
      for (const auto &name : lc.data->localTexNames) {
        if (name == texName) {
          usesTexture = true;
          break;
        }
      }
      if (!usesTexture) {
        for (const auto &name : lc.data->globalTexNames) {
          if (name == texName) {
            usesTexture = true;
            break;
          }
        }
      }
      if (usesTexture) {
        sceneViewport.RebuildChunkBatches(lc.data->chunkName, workspaceDir);
        if (!m_sharedChunks) {
          RebuildChunkBatches(lc.data->chunkName, workspaceDir);
        }
      }
    }
  }
}

void LocalGeometryOverlay::HandlePicking(Viewport &vp, Ray ray) {
  if (m_texManager && m_texManager->IsTilePaintModeActive() && m_editMode == EditMode::Face) {
    return;
  }

  float closestDist = FLT_MAX;
  std::string hitChunk;
  int hitObjIdx = -1;
  int hitMeshIdx = -1;
  int hitFaceIdx = -1;
  int hitVertexIdx = -1;

  for (const auto &lc : GetChunks()) {
    if (!lc.visible || lc.hasError)
      continue;

    for (size_t i = 0; i < lc.data->objects.size(); ++i) {
      const auto &obj = lc.data->objects[i];
      if (m_editMode == EditMode::GlobalObject && !obj.isGlobal)
        continue;
      if (m_editMode != EditMode::GlobalObject && obj.isGlobal)
        continue;

      bool insideBox = (vp.GetCamera().position.x >= obj.bounds.min.x &&
                        vp.GetCamera().position.x <= obj.bounds.max.x &&
                        vp.GetCamera().position.y >= obj.bounds.min.y &&
                        vp.GetCamera().position.y <= obj.bounds.max.y &&
                        vp.GetCamera().position.z >= obj.bounds.min.z &&
                        vp.GetCamera().position.z <= obj.bounds.max.z);

      if (!insideBox) {
        RayCollision boxHit = GetRayCollisionBox(ray, obj.bounds);
        if (!boxHit.hit)
          continue;
      }

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
              hitMeshIdx = (int)mi;

              if (m_editMode == EditMode::Face) {
                hitFaceIdx = (int)fi;
              }
            }
          }
        }
      }
    }
  }

  if (m_editMode == EditMode::Vertex) {
    float bestDist3D = FLT_MAX;
    std::string bestChunk;
    int bestObjIdx = -1;
    int bestMeshIdx = -1;
    int bestVertexIdx = -1;

    Vector3 forward =
        Vector3Normalize({vp.GetCamera().target.x - vp.GetCamera().position.x,
                          vp.GetCamera().target.y - vp.GetCamera().position.y,
                          vp.GetCamera().target.z - vp.GetCamera().position.z});

    for (const auto &lc : GetChunks()) {
      if (!lc.visible || lc.hasError)
        continue;
      for (size_t objIdx = 0; objIdx < lc.data->objects.size(); ++objIdx) {
        const auto &obj = lc.data->objects[objIdx];
        if (obj.bounds.min.x > obj.bounds.max.x)
          continue;
        if (obj.isGlobal)
          continue;
        for (size_t meshIdx = 0; meshIdx < obj.meshes.size(); ++meshIdx) {
          const auto &mesh = obj.meshes[meshIdx];
          for (size_t vIdx = 0; vIdx < mesh.vx.size(); ++vIdx) {
            Vector3 v = {mesh.vx[vIdx], mesh.vy[vIdx], mesh.vz[vIdx]};
            Vector3 toV = {v.x - vp.GetCamera().position.x, v.y - vp.GetCamera().position.y,
                           v.z - vp.GetCamera().position.z};
            if (Vector3DotProduct(forward, toV) > 0.1f) {
              Vector2 screenPos =
                  GetWorldToScreenEx(v, vp.GetCamera(), vp.GetWidth(), vp.GetHeight());
              float dist2D = Vector2Distance(screenPos, vp.GetLocalMousePos());
              if (dist2D < 15.0f) {
                float dist3D = Vector3Distance(vp.GetCamera().position, v);
                if (dist3D <=
                    closestDist + 0.5f) {
                  if (dist3D < bestDist3D) {
                    bestDist3D = dist3D;
                    bestChunk = lc.data->chunkName;
                    bestObjIdx = (int)objIdx;
                    bestMeshIdx = (int)meshIdx;
                    bestVertexIdx = (int)vIdx;
                  }
                }
              }
            }
          }
        }
      }
    }

    ImGuiIO &io = ImGui::GetIO();
    bool isRightClick = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    bool isMultiselect = Config::Get().IsMultiselectDown();

    if (bestVertexIdx >= 0) {
      m_selectedChunk = bestChunk;
      m_selectedObjectIdx = bestObjIdx;
      m_selectedMeshIdx = bestMeshIdx;
      m_selectedVertexIdx = bestVertexIdx;

      SelectedVertex v = {bestChunk, bestObjIdx, bestMeshIdx, bestVertexIdx};
      bool alreadySelected = m_selectedVertices.find(v) != m_selectedVertices.end();

      if (isRightClick && alreadySelected) {
          // Keep selection group intact
      } else {
          if (!isMultiselect) m_selectedVertices.clear();

          if (isMultiselect && alreadySelected && !isRightClick) {
            m_selectedVertices.erase(v);
          } else {
            m_selectedVertices.insert(v);
          }
      }
    } else {
      if (!isMultiselect) {
        m_selectedChunk = "";
        m_selectedVertices.clear();
        m_selectedMeshIdx = -1;
        m_selectedObjectIdx = -1;
        m_selectedVertexIdx = -1;
      }
    }
    return;
  }

  ImGuiIO &io = ImGui::GetIO();
  bool isRightClick = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  bool isMultiselect = Config::Get().IsMultiselectDown();

  if (hitFaceIdx >= 0) {
    m_selectedChunk = hitChunk;
    m_selectedObjectIdx = hitObjIdx;
    m_selectedMeshIdx = hitMeshIdx;
    m_selectedFaceIdx = hitFaceIdx;

    SelectedFace f = {hitChunk, hitObjIdx, hitMeshIdx, hitFaceIdx};
    bool alreadySelected = m_selectedFaces.find(f) != m_selectedFaces.end();

    if (isRightClick && alreadySelected) {
        // Keep selection group intact
    } else {
        if (!isMultiselect) m_selectedFaces.clear();

        if (isMultiselect && alreadySelected && !isRightClick) {
          m_selectedFaces.erase(f);
        } else {
          m_selectedFaces.insert(f);
        }
    }
  } else if (hitObjIdx >= 0 && (m_editMode == EditMode::Mesh || m_editMode == EditMode::GlobalObject)) {
    m_selectedChunk = hitChunk;
    m_selectedObjectIdx = hitObjIdx;
    m_selectedMeshIdx = hitMeshIdx;
    m_selectedFaceIdx = hitFaceIdx;
  } else if (!isMultiselect) {
    m_selectedChunk = "";
    m_selectedObjectIdx = -1;
    m_selectedMeshIdx = -1;
    m_selectedFaceIdx = -1;
    m_selectedFaces.clear();
  }
}

void LocalGeometryOverlay::HandleBoxPicking(Viewport &vp, Rectangle box) {
  if (m_editMode != EditMode::Vertex && m_editMode != EditMode::Face)
    return;

  if (box.width < 1.0f || box.height < 1.0f)
    return;

  bool isMultiselect = Config::Get().IsMultiselectDown();

  Vector3 camPos = vp.GetCamera().position;
  Vector3 forward = Vector3Normalize({vp.GetCamera().target.x - vp.GetCamera().position.x,
                                      vp.GetCamera().target.y - vp.GetCamera().position.y,
                                      vp.GetCamera().target.z - vp.GetCamera().position.z});

  if (m_editMode == EditMode::Vertex) {
    if (!isMultiselect) {
      m_selectedVertices.clear();
    }

    struct CandidateVertex {
      Vector3 pos;
      float dist;
      Ray ray;
      const std::string *chunkName;
      int objIdx;
      int meshIdx;
      int vIdx;
      bool occluded;
    };

    std::vector<CandidateVertex> candidates;
    BoundingBox raysAABB;
    raysAABB.min = camPos;
    raysAABB.max = camPos;

    for (const auto &lc : GetChunks()) {
      if (!lc.visible || lc.hasError)
        continue;

      for (size_t objIdx = 0; objIdx < lc.data->objects.size(); ++objIdx) {
        const auto &obj = lc.data->objects[objIdx];
        if (obj.bounds.min.x > obj.bounds.max.x)
          continue;
        if (obj.isGlobal)
          continue;

        for (size_t meshIdx = 0; meshIdx < obj.meshes.size(); ++meshIdx) {
          const auto &mesh = obj.meshes[meshIdx];

          for (size_t vIdx = 0; vIdx < mesh.vx.size(); ++vIdx) {
            Vector3 v = {mesh.vx[vIdx], mesh.vy[vIdx], mesh.vz[vIdx]};

            Vector3 toV = {v.x - camPos.x, v.y - camPos.y, v.z - camPos.z};
            if (Vector3DotProduct(forward, toV) < 0.1f)
              continue;

            Vector2 screenPos =
                GetWorldToScreenEx(v, vp.GetCamera(), vp.GetWidth(), vp.GetHeight());
            if (screenPos.x >= box.x && screenPos.x <= box.x + box.width &&
                screenPos.y >= box.y && screenPos.y <= box.y + box.height) {
              CandidateVertex cand;
              cand.pos = v;
              cand.dist = Vector3Length(toV);
              cand.ray = {camPos, Vector3Normalize(toV)};
              cand.chunkName = &lc.data->chunkName;
              cand.objIdx = (int)objIdx;
              cand.meshIdx = (int)meshIdx;
              cand.vIdx = (int)vIdx;
              cand.occluded = false;
              candidates.push_back(cand);

              raysAABB.min = Vector3Min(raysAABB.min, v);
              raysAABB.max = Vector3Max(raysAABB.max, v);
            }
          }
        }
      }
    }

    if (candidates.empty())
      return;

    for (const auto &olc : GetChunks()) {
      if (!olc.visible || olc.hasError)
        continue;
      for (const auto &oobj : olc.data->objects) {
        if (oobj.bounds.min.x > oobj.bounds.max.x)
          continue;

        if (!CheckCollisionBoxes(oobj.bounds, raysAABB))
          continue;

        std::vector<CandidateVertex *> hitsThisObject;
        for (auto &cand : candidates) {
          if (cand.occluded)
            continue;
          RayCollision boxHit = GetRayCollisionBox(cand.ray, oobj.bounds);
          if (boxHit.hit && boxHit.distance < cand.dist) {
            hitsThisObject.push_back(&cand);
          }
        }
        if (hitsThisObject.empty())
          continue;

        for (const auto &omesh : oobj.meshes) {
          for (const auto &oface : omesh.faces) {
            bool isQuad = (oface.v[3] != 0xFF);
            int triCount = isQuad ? 2 : 1;
            static const int triV[2][3] = {{0, 1, 2}, {0, 2, 3}};

            for (int t = 0; t < triCount; ++t) {
              Vector3 v1 = {omesh.vx[oface.v[triV[t][0]]],
                            omesh.vy[oface.v[triV[t][0]]],
                            omesh.vz[oface.v[triV[t][0]]]};
              Vector3 v2 = {omesh.vx[oface.v[triV[t][1]]],
                            omesh.vy[oface.v[triV[t][1]]],
                            omesh.vz[oface.v[triV[t][1]]]};
              Vector3 v3 = {omesh.vx[oface.v[triV[t][2]]],
                            omesh.vy[oface.v[triV[t][2]]],
                            omesh.vz[oface.v[triV[t][2]]]};

              for (auto *cand : hitsThisObject) {
                if (cand->occluded)
                  continue;
                RayCollision triHit =
                    GetRayCollisionTriangle(cand->ray, v1, v2, v3);
                if (triHit.hit &&
                    triHit.distance < cand->dist - 0.5f) {
                  cand->occluded = true;
                }
              }
            }
          }
        }
      }
    }

    bool anyAdded = false;
    for (const auto &cand : candidates) {
      if (!cand.occluded) {
        m_selectedVertices.insert(
            {*cand.chunkName, cand.objIdx, cand.meshIdx, cand.vIdx});
        if (!anyAdded) {
          m_selectedChunk = *cand.chunkName;
          m_selectedObjectIdx = cand.objIdx;
          m_selectedMeshIdx = cand.meshIdx;
          m_selectedVertexIdx = cand.vIdx;
          anyAdded = true;
        }
      }
    }
  } else if (m_editMode == EditMode::Face) {
    if (!isMultiselect) {
      m_selectedFaces.clear();
    }

    struct CandidateFace {
      Vector3 center;
      Vector3 samplePoints[5];
      int sampleCount;
      float dist;
      Ray ray;
      const std::string *chunkName;
      int objIdx;
      int meshIdx;
      int faceIdx;
      bool occluded;
    };

    std::vector<CandidateFace> candidates;
    BoundingBox raysAABB;
    raysAABB.min = camPos;
    raysAABB.max = camPos;

    for (const auto &lc : GetChunks()) {
      if (!lc.visible || lc.hasError)
        continue;

      for (size_t objIdx = 0; objIdx < lc.data->objects.size(); ++objIdx) {
        const auto &obj = lc.data->objects[objIdx];
        if (obj.bounds.min.x > obj.bounds.max.x)
          continue;
        if (obj.isGlobal)
          continue;

        for (size_t meshIdx = 0; meshIdx < obj.meshes.size(); ++meshIdx) {
          const auto &mesh = obj.meshes[meshIdx];

          for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
            const auto &face = mesh.faces[fi];
            bool isQuad = (face.v[3] != 0xFF);

            Vector3 v0 = {mesh.vx[face.v[0]], mesh.vy[face.v[0]], mesh.vz[face.v[0]]};
            Vector3 v1 = {mesh.vx[face.v[1]], mesh.vy[face.v[1]], mesh.vz[face.v[1]]};
            Vector3 v2 = {mesh.vx[face.v[2]], mesh.vy[face.v[2]], mesh.vz[face.v[2]]};
            Vector3 v3 = isQuad ? Vector3{mesh.vx[face.v[3]], mesh.vy[face.v[3]], mesh.vz[face.v[3]]} : Vector3{0.0f, 0.0f, 0.0f};

            Vector3 faceCenter;
            if (isQuad) {
              faceCenter = {(v0.x + v1.x + v2.x + v3.x) * 0.25f,
                            (v0.y + v1.y + v2.y + v3.y) * 0.25f,
                            (v0.z + v1.z + v2.z + v3.z) * 0.25f};
            } else {
              faceCenter = {(v0.x + v1.x + v2.x) / 3.0f,
                            (v0.y + v1.y + v2.y) / 3.0f,
                            (v0.z + v1.z + v2.z) / 3.0f};
            }

            Vector3 toCenter = {faceCenter.x - camPos.x, faceCenter.y - camPos.y, faceCenter.z - camPos.z};
            Vector3 toV0 = {v0.x - camPos.x, v0.y - camPos.y, v0.z - camPos.z};
            Vector3 toV1 = {v1.x - camPos.x, v1.y - camPos.y, v1.z - camPos.z};
            Vector3 toV2 = {v2.x - camPos.x, v2.y - camPos.y, v2.z - camPos.z};
            Vector3 toV3 = isQuad ? Vector3{v3.x - camPos.x, v3.y - camPos.y, v3.z - camPos.z} : Vector3{0.0f, 0.0f, 0.0f};

            // Face must be strictly in front of the camera plane
            if (Vector3DotProduct(forward, toCenter) < 0.1f ||
                Vector3DotProduct(forward, toV0) < 0.1f ||
                Vector3DotProduct(forward, toV1) < 0.1f ||
                Vector3DotProduct(forward, toV2) < 0.1f ||
                (isQuad && Vector3DotProduct(forward, toV3) < 0.1f)) {
              continue;
            }

            Vector2 s0 = GetWorldToScreenEx(v0, vp.GetCamera(), vp.GetWidth(), vp.GetHeight());
            Vector2 s1 = GetWorldToScreenEx(v1, vp.GetCamera(), vp.GetWidth(), vp.GetHeight());
            Vector2 s2 = GetWorldToScreenEx(v2, vp.GetCamera(), vp.GetWidth(), vp.GetHeight());
            Vector2 s3 = isQuad ? GetWorldToScreenEx(v3, vp.GetCamera(), vp.GetWidth(), vp.GetHeight()) : Vector2{0.0f, 0.0f};
            Vector2 sCenter = GetWorldToScreenEx(faceCenter, vp.GetCamera(), vp.GetWidth(), vp.GetHeight());

            bool hit = false;

            // 1. Centroid inside selection box
            if (sCenter.x >= box.x && sCenter.x <= box.x + box.width &&
                sCenter.y >= box.y && sCenter.y <= box.y + box.height) {
              hit = true;
            }

            // 2. Any edge crosses selection box (or any vertex inside box)
            if (!hit) {
              if (SegmentIntersectsBox2D(s0, s1, box) ||
                  SegmentIntersectsBox2D(s1, s2, box) ||
                  (isQuad ? (SegmentIntersectsBox2D(s2, s3, box) || SegmentIntersectsBox2D(s3, s0, box))
                          : SegmentIntersectsBox2D(s2, s0, box))) {
                hit = true;
              }
            }

            // 3. Selection box center inside face
            if (!hit) {
              Vector2 boxCenter = {box.x + box.width * 0.5f, box.y + box.height * 0.5f};
              if (PointInTriangle2D(boxCenter, s0, s1, s2) ||
                  (isQuad && PointInTriangle2D(boxCenter, s0, s2, s3))) {
                hit = true;
              }
            }

            if (hit) {
              CandidateFace cand;
              cand.center = faceCenter;
              cand.samplePoints[0] = faceCenter;
              cand.samplePoints[1] = v0;
              cand.samplePoints[2] = v1;
              cand.samplePoints[3] = v2;
              cand.sampleCount = 4;
              if (isQuad) {
                cand.samplePoints[4] = v3;
                cand.sampleCount = 5;
              }
              cand.dist = Vector3Length(toCenter);
              cand.ray = {camPos, Vector3Normalize(toCenter)};
              cand.chunkName = &lc.data->chunkName;
              cand.objIdx = (int)objIdx;
              cand.meshIdx = (int)meshIdx;
              cand.faceIdx = (int)fi;
              cand.occluded = false;
              candidates.push_back(cand);

              raysAABB.min = Vector3Min(raysAABB.min, faceCenter);
              raysAABB.max = Vector3Max(raysAABB.max, faceCenter);
              raysAABB.min = Vector3Min(raysAABB.min, v0);
              raysAABB.max = Vector3Max(raysAABB.max, v0);
              raysAABB.min = Vector3Min(raysAABB.min, v1);
              raysAABB.max = Vector3Max(raysAABB.max, v1);
              raysAABB.min = Vector3Min(raysAABB.min, v2);
              raysAABB.max = Vector3Max(raysAABB.max, v2);
              if (isQuad) {
                raysAABB.min = Vector3Min(raysAABB.min, v3);
                raysAABB.max = Vector3Max(raysAABB.max, v3);
              }
            }
          }
        }
      }
    }

    if (candidates.empty())
      return;

    for (const auto &olc : GetChunks()) {
      if (!olc.visible || olc.hasError)
        continue;
      for (size_t oobjIdx = 0; oobjIdx < olc.data->objects.size(); ++oobjIdx) {
        const auto &oobj = olc.data->objects[oobjIdx];
        if (oobj.bounds.min.x > oobj.bounds.max.x)
          continue;

        if (!CheckCollisionBoxes(oobj.bounds, raysAABB))
          continue;

        std::vector<CandidateFace *> hitsThisObject;
        for (auto &cand : candidates) {
          if (cand.occluded)
            continue;
          RayCollision boxHit = GetRayCollisionBox(cand.ray, oobj.bounds);
          if (boxHit.hit && boxHit.distance < cand.dist) {
            hitsThisObject.push_back(&cand);
          }
        }
        if (hitsThisObject.empty())
          continue;

        for (size_t omeshIdx = 0; omeshIdx < oobj.meshes.size(); ++omeshIdx) {
          const auto &omesh = oobj.meshes[omeshIdx];
          for (size_t ofaceIdx = 0; ofaceIdx < omesh.faces.size(); ++ofaceIdx) {
            const auto &oface = omesh.faces[ofaceIdx];
            bool isQuad = (oface.v[3] != 0xFF);
            int triCount = isQuad ? 2 : 1;
            static const int triV[2][3] = {{0, 1, 2}, {0, 2, 3}};

            for (int t = 0; t < triCount; ++t) {
              Vector3 v1 = {omesh.vx[oface.v[triV[t][0]]],
                            omesh.vy[oface.v[triV[t][0]]],
                            omesh.vz[oface.v[triV[t][0]]]};
              Vector3 v2 = {omesh.vx[oface.v[triV[t][1]]],
                            omesh.vy[oface.v[triV[t][1]]],
                            omesh.vz[oface.v[triV[t][1]]]};
              Vector3 v3 = {omesh.vx[oface.v[triV[t][2]]],
                            omesh.vy[oface.v[triV[t][2]]],
                            omesh.vz[oface.v[triV[t][2]]]};

              for (auto *cand : hitsThisObject) {
                if (cand->occluded)
                  continue;
                if (*cand->chunkName == olc.data->chunkName &&
                    cand->objIdx == (int)oobjIdx &&
                    cand->meshIdx == (int)omeshIdx &&
                    cand->faceIdx == (int)ofaceIdx) {
                  continue;
                }
                RayCollision triHit =
                    GetRayCollisionTriangle(cand->ray, v1, v2, v3);
                if (triHit.hit &&
                    triHit.distance < cand->dist - 0.5f) {
                  cand->occluded = true;
                }
              }
            }
          }
        }
      }
    }

    bool anyAdded = false;
    for (const auto &cand : candidates) {
      if (!cand.occluded) {
        m_selectedFaces.insert(
            {*cand.chunkName, cand.objIdx, cand.meshIdx, cand.faceIdx});
        if (!anyAdded) {
          m_selectedChunk = *cand.chunkName;
          m_selectedObjectIdx = cand.objIdx;
          m_selectedMeshIdx = cand.meshIdx;
          m_selectedFaceIdx = cand.faceIdx;
          anyAdded = true;
        }
      }
    }
  }
}

void LocalGeometryOverlay::TranslateSelection(Vector3 delta) {
  bool modified = Geometry::TranslateSelection(
      delta,
      m_editMode,
      m_selectedChunk,
      m_selectedObjectIdx,
      m_selectedVertices,
      m_selectedFaces,
      m_sharedChunks ? const_cast<std::vector<LoadedChunk>&>(*m_sharedChunks) : m_chunks,
      m_autoValidate,
      m_history,
      m_lastWorkspaceDir
  );

  if (modified) {
      if (m_editMode == EditMode::Vertex || m_editMode == EditMode::Face) {
          // Find which chunks were modified to rebuild batches
          std::set<std::string> chunksToRebuild;
          if (m_editMode == EditMode::Vertex) {
              for (const auto &sv : m_selectedVertices) chunksToRebuild.insert(sv.chunkName);
          } else {
              for (const auto &sf : m_selectedFaces) chunksToRebuild.insert(sf.chunkName);
          }
          for (const auto& cName : chunksToRebuild) {
              RebuildChunkBatches(cName, m_lastWorkspaceDir);
          }
      } else if (m_editMode == EditMode::Mesh || m_editMode == EditMode::GlobalObject) {
          RebuildChunkBatches(m_selectedChunk, m_lastWorkspaceDir);
      }
  }
}

void LocalGeometryOverlay::HandleTilePainting(Viewport &vp, Ray ray) {
  if (!m_texManager) return;
  float closestDist = FLT_MAX;
  std::string hitChunk;
  int hitObjIdx = -1;
  int hitMeshIdx = -1;
  int hitFaceIdx = -1;

  for (const auto &lcConst : GetChunks()) {
    auto &lc = const_cast<LoadedChunk &>(lcConst);
    if (!lc.visible || lc.hasError) continue;
    for (size_t i = 0; i < lc.data->objects.size(); ++i) {
      const auto &obj = lc.data->objects[i];
      if (obj.bounds.min.x > obj.bounds.max.x) continue;

      bool insideBox = (vp.GetCamera().position.x >= obj.bounds.min.x &&
                        vp.GetCamera().position.x <= obj.bounds.max.x &&
                        vp.GetCamera().position.y >= obj.bounds.min.y &&
                        vp.GetCamera().position.y <= obj.bounds.max.y &&
                        vp.GetCamera().position.z >= obj.bounds.min.z &&
                        vp.GetCamera().position.z <= obj.bounds.max.z);

      if (!insideBox) {
        RayCollision boxHit = GetRayCollisionBox(ray, obj.bounds);
        if (!boxHit.hit) continue;
      }

      for (size_t mi = 0; mi < obj.meshes.size(); ++mi) {
        const auto &mesh = obj.meshes[mi];
        for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
          const auto &face = mesh.faces[fi];
          bool isQuad = (face.v[3] != 0xFF);
          int triCount = isQuad ? 2 : 1;
          static const int triV[2][3] = {{0, 1, 2}, {0, 2, 3}};

          for (int t = 0; t < triCount; ++t) {
            Vector3 v1 = {mesh.vx[face.v[triV[t][0]]], mesh.vy[face.v[triV[t][0]]], mesh.vz[face.v[triV[t][0]]]};
            Vector3 v2 = {mesh.vx[face.v[triV[t][1]]], mesh.vy[face.v[triV[t][1]]], mesh.vz[face.v[triV[t][1]]]};
            Vector3 v3 = {mesh.vx[face.v[triV[t][2]]], mesh.vy[face.v[triV[t][2]]], mesh.vz[face.v[triV[t][2]]]};

            RayCollision triHit = GetRayCollisionTriangle(ray, v1, v2, v3);
            if (triHit.hit && triHit.distance < closestDist) {
              closestDist = triHit.distance;
              hitChunk = lc.data->chunkName;
              hitObjIdx = (int)i;
              hitMeshIdx = (int)mi;
              hitFaceIdx = (int)fi;
            }
          }
        }
      }
    }
  }

  if (hitFaceIdx != -1) {
    const auto& currentTile = m_texManager->GetCurrentTile();
    if (currentTile.texName.empty()) return; // Nothing to paint

    ParsedChunk* cd = nullptr;
    for (const auto &lcConst : GetChunks()) {
      auto &lc = const_cast<LoadedChunk &>(lcConst);
      if (lc.data->chunkName == hitChunk) {
        cd = lc.data.get();
        break;
      }
    }
    
    if (cd && hitObjIdx < cd->objects.size()) {
      auto& obj = cd->objects[hitObjIdx];
      if (hitMeshIdx < obj.meshes.size()) {
        auto& mesh = obj.meshes[hitMeshIdx];
        if (hitFaceIdx < mesh.faces.size()) {
          auto& face = mesh.faces[hitFaceIdx];
          
          float minU = currentTile.minU;
          float minV = currentTile.minV;
          float maxU = currentTile.maxU;
          float maxV = currentTile.maxV;
          
          uint8_t texNum = 0x7F;
          const auto& texList = obj.isGlobal ? cd->globalTexNames : cd->localTexNames;
          for (size_t i = 0; i < texList.size(); i++) {
              if (texList[i] == currentTile.texName) {
                  texNum = (uint8_t)i;
                  break;
              }
          }
          
          RenderMesh snapBefore = mesh;
          
          face.texName = currentTile.texName;
          face.texNum = texNum;
          face.paletteRow = currentTile.palette;
          
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
          
          bool changed = false;
          if (snapBefore.faces[hitFaceIdx].texName != face.texName ||
              snapBefore.faces[hitFaceIdx].paletteRow != face.paletteRow) {
              changed = true;
          } else {
              for(int i=0; i<numVerts; i++) {
                  if (snapBefore.faces[hitFaceIdx].uv[i][0] != face.uv[i][0] ||
                      snapBefore.faces[hitFaceIdx].uv[i][1] != face.uv[i][1]) {
                      changed = true; break;
                  }
              }
          }
          
          if (changed) {
              if (m_history) {
                  m_history->Push({hitChunk, hitObjIdx, hitMeshIdx, snapBefore, mesh, "Tile Paint"});
              }
              RebuildChunkBatches(hitChunk, m_lastWorkspaceDir);
          }
        }
      }
    }
  }
}

void LocalGeometryOverlay::DrawContextMenu() {
  if (m_editMode == EditMode::GlobalObject) {
    if (m_selectedChunk.empty() || m_selectedObjectIdx == -1) return;
    if (ImGui::MenuItem("Duplicate Prop")) {
      Geometry::DuplicateGlobalObject(*this, nullptr);
    }
    if (ImGui::MenuItem("Delete Prop")) {
      Geometry::DeleteGlobalObject(*this, nullptr);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Snap to Floor (Y=0)")) {
      Geometry::SnapGlobalObjectToFloor(*this, nullptr);
    }
    if (ImGui::MenuItem("Snap to Grid")) {
      Geometry::SnapGlobalObjectToGrid(*this, nullptr);
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Mirror Prop")) {
      if (ImGui::MenuItem("X")) { Geometry::MirrorGlobalObject(*this, 0, nullptr); }
      if (ImGui::MenuItem("Y")) { Geometry::MirrorGlobalObject(*this, 1, nullptr); }
      if (ImGui::MenuItem("Z")) { Geometry::MirrorGlobalObject(*this, 2, nullptr); }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Rotate Prop")) {
      if (ImGui::MenuItem("+Yaw (90*)")) { Geometry::RotateGlobalObject(*this, 1, nullptr); }
      if (ImGui::MenuItem("-Yaw (90*)")) { Geometry::RotateGlobalObject(*this, -1, nullptr); }
      if (ImGui::MenuItem("180*")) { Geometry::RotateGlobalObject(*this, 2, nullptr); }
      ImGui::EndMenu();
    }
  } else if (m_editMode == EditMode::Mesh) {
    if (m_selectedChunk.empty() || m_selectedObjectIdx == -1) return;
    if (ImGui::MenuItem("Duplicate Mesh")) {
      Geometry::DuplicateMesh(*this, nullptr);
    }
    if (ImGui::MenuItem("Delete Mesh")) {
      Geometry::DeleteMesh(*this, nullptr);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Separate to New Object")) {
      Geometry::SeparateMeshToNewObject(*this, nullptr);
    }
    if (ImGui::MenuItem("Merge Selected Meshes")) {
      Geometry::MergeMeshes(*this, nullptr);
    }
    if (ImGui::MenuItem("Snap Mesh to Floor")) {
      Geometry::SnapMeshToFloor(*this, nullptr);
    }
    if (ImGui::MenuItem("Center Mesh Pivot")) {
      Geometry::CenterMeshPivot(*this, nullptr);
    }
    if (ImGui::MenuItem("Recalculate Bounds")) {
      Geometry::RecalculateBounds(*this);
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Mirror Mesh")) {
      if (ImGui::MenuItem("X")) { Geometry::MirrorMesh(*this, 0, nullptr); }
      if (ImGui::MenuItem("Y")) { Geometry::MirrorMesh(*this, 1, nullptr); }
      if (ImGui::MenuItem("Z")) { Geometry::MirrorMesh(*this, 2, nullptr); }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Rotate Mesh")) {
      if (ImGui::MenuItem("+X (90)")) { Geometry::RotateMesh(*this, 0, 90.0f, nullptr); }
      if (ImGui::MenuItem("-X (90)")) { Geometry::RotateMesh(*this, 0, -90.0f, nullptr); }
      if (ImGui::MenuItem("+Y (90)")) { Geometry::RotateMesh(*this, 1, 90.0f, nullptr); }
      if (ImGui::MenuItem("-Y (90)")) { Geometry::RotateMesh(*this, 1, -90.0f, nullptr); }
      if (ImGui::MenuItem("+Z (90)")) { Geometry::RotateMesh(*this, 2, 90.0f, nullptr); }
      if (ImGui::MenuItem("-Z (90)")) { Geometry::RotateMesh(*this, 2, -90.0f, nullptr); }
      ImGui::EndMenu();
    }
  } else if (m_editMode == EditMode::Face) {
    if (m_selectedFaces.empty() && m_selectedFaceIdx == -1) return;
    
    bool hasTexture = false;
    for (const auto &sf : m_selectedFaces) {
      for (const auto &lc : GetChunks()) {
        if (lc.data->chunkName == sf.chunkName && sf.objectIdx >= 0 && sf.objectIdx < (int)lc.data->objects.size()) {
          const auto &obj = lc.data->objects[sf.objectIdx];
          if (sf.meshIdx >= 0 && sf.meshIdx < (int)obj.meshes.size()) {
            const auto &mesh = obj.meshes[sf.meshIdx];
            if (sf.faceIdx >= 0 && sf.faceIdx < (int)mesh.faces.size()) {
               if (!mesh.faces[sf.faceIdx].texName.empty() || mesh.faces[sf.faceIdx].texNum != 0x7F) {
                  hasTexture = true;
               }
            }
          }
        }
      }
      if (hasTexture) break;
    }

    if (ImGui::MenuItem("Subdivide")) {
      Geometry::SubdivideSelectedFaces(*this);
    }
    if (ImGui::MenuItem("Triangulate")) {
      Geometry::TriangulateFaces(*this, m_history);
    }
    if (ImGui::MenuItem("Connect / Bridge")) {
      Geometry::ConnectBridgeFaces(*this, m_history);
    }
    if (ImGui::MenuItem("Invert Normals")) {
      Geometry::InvertNormals(*this, m_history);
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Delete")) {
      if (ImGui::MenuItem("Delete Face")) {
        Geometry::DeleteFaces(*this, false, m_history);
      }
      if (ImGui::MenuItem("Delete Face and Vertices")) {
        Geometry::DeleteFaces(*this, true, m_history);
      }
      ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Extrude")) {
      Geometry::ExtrudeFaces(*this, 0.0f, 0, m_history);
    }
    
    if (hasTexture) {
        ImGui::Separator();
        if (ImGui::BeginMenu("Mirror Texture")) {
          if (ImGui::MenuItem("Horizontal")) {
            Geometry::FlipUV(*this, true, false, m_history);
          }
          if (ImGui::MenuItem("Vertical")) {
            Geometry::FlipUV(*this, false, true, m_history);
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Rotate Texture")) {
          if (ImGui::MenuItem("90*", ",")) {
            Geometry::RotateUV(*this, 1, m_history);
          }
          if (ImGui::MenuItem("180*")) {
            Geometry::RotateUV(*this, 2, m_history);
          }
          if (ImGui::MenuItem("270*", ".")) {
            Geometry::RotateUV(*this, 3, m_history);
          }
          ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Fit UV to Tile Bounds")) {
          Geometry::FitUVToTileBounds(*this, m_history);
        }
        if (ImGui::MenuItem("Reset Default UV")) {
          Geometry::ResetDefaultUV(*this, m_history);
        }
        if (ImGui::MenuItem("Remove Texture")) {
          Geometry::ClearTexture(*this, m_history);
        }
    }
  } else if (m_editMode == EditMode::Vertex) {
    bool hasSelection = (!m_selectedVertices.empty() || m_selectedVertexIdx != -1);
    if (!hasSelection) return;

    if (ImGui::MenuItem("Snap to Grid")) {
      Geometry::SnapVerticesToGrid(*this, m_history);
    }
    if (ImGui::MenuItem("Snap to Floor (Y=0)")) {
      Geometry::SnapVerticesToFloor(*this, m_history);
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Flatten / Planarize")) {
      if (ImGui::MenuItem("Flatten X")) { Geometry::PlanarizeVertices(*this, 0, m_history); }
      if (ImGui::MenuItem("Flatten Y")) { Geometry::PlanarizeVertices(*this, 1, m_history); }
      if (ImGui::MenuItem("Flatten Z")) { Geometry::PlanarizeVertices(*this, 2, m_history); }
      ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Add Face from Vertices (3-4)")) {
      Geometry::AddFaceFromSelectedVertices(*this, m_history);
    }
    if (ImGui::MenuItem("Extrude Vertices")) {
      int rawUnits = 1 << m_moveStepPower;
      float step = (float)rawUnits / 256.0f;
      Geometry::ExtrudeSelectedVertices(*this, { 0.0f, step, 0.0f }, m_history);
    }
    if (ImGui::MenuItem("Weld Vertices")) {
      Geometry::WeldVertices(*this, 0.05f, m_history);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete Vertices")) {
      Geometry::DeleteSelectedVertices(*this, m_history);
    }
  }
}

