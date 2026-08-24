#include "viewport/CollisionOverlay.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <map>

#include "viewport/Viewport.h"

void CollisionOverlay::HandlePicking(Viewport& vp, Ray ray) {
  // Stub for future collision data selection
  // Allows selecting collision cells or surfaces by intersecting ray with
  // CollisionBatch meshes.
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

CollisionOverlay::~CollisionOverlay() { UnloadAll(); }

void CollisionOverlay::UnloadAll() {
  for (auto &c : m_chunks) {
    FreeCollisionBatches(c);
  }
  m_chunks.clear();
}

bool CollisionOverlay::LoadChunk(const ParsedChunk &parsedChunk) {
  if (!parsedChunk.collision.hasCollision)
    return false;

  UnloadChunk(parsedChunk.chunkName);

  CollisionChunkData data;
  data.chunkName = parsedChunk.chunkName;
  data.xPos = parsedChunk.xPos;
  data.yPos = parsedChunk.yPos;
  data.bounds = {{99999.0f, 99999.0f, 99999.0f},
                 {-99999.0f, -99999.0f, -99999.0f}};
  data.splitVertices.reserve(parsedChunk.collision.splitVertices.size());

  // Build Collision Geometry
  if (parsedChunk.collision.hasCollision) {
    BuildCollisionBatches(data, parsedChunk.collision);
  }

  // Build Visual Geometry (Flat Grey Faces + Black Wireframes)
  ParsedChunk tempChunk = parsedChunk;
  IPDParse::BuildBatches(tempChunk);
  for (const auto &batch : tempChunk.batches) {
    if (batch.vertexCount == 0)
      continue;

    CollisionBatch cb;
    cb.mesh = {0};
    cb.mesh.vertexCount = batch.vertexCount;
    cb.mesh.triangleCount = batch.vertexCount / 3;
    cb.mesh.vertices =
        (float *)MemAlloc(batch.positions.size() * sizeof(float));
    memcpy(cb.mesh.vertices, batch.positions.data(),
           batch.positions.size() * sizeof(float));
    cb.mesh.colors = (unsigned char *)MemAlloc(batch.vertexCount * 4 *
                                               sizeof(unsigned char));
    for (int i = 0; i < batch.vertexCount; ++i) {
      cb.mesh.colors[i * 4 + 0] = 255;
      cb.mesh.colors[i * 4 + 1] = 255;
      cb.mesh.colors[i * 4 + 2] = 255;
      cb.mesh.colors[i * 4 + 3] = 255;
    }

    UploadMesh(&cb.mesh, false);
    cb.meshUploaded = true;

    BoundingBox meshBox = GetMeshBoundingBox(cb.mesh);
    data.bounds.min.x = std::min(data.bounds.min.x, meshBox.min.x);
    data.bounds.min.y = std::min(data.bounds.min.y, meshBox.min.y);
    data.bounds.min.z = std::min(data.bounds.min.z, meshBox.min.z);
    data.bounds.max.x = std::max(data.bounds.max.x, meshBox.max.x);
    data.bounds.max.y = std::max(data.bounds.max.y, meshBox.max.y);
    data.bounds.max.z = std::max(data.bounds.max.z, meshBox.max.z);

    cb.material = LoadMaterialDefault();
    cb.material.maps[MATERIAL_MAP_ALBEDO].color = {160, 160, 160,
                                                   255}; // Light grey

    data.visualBatches.push_back(std::move(cb));
  }

  m_chunks.push_back(std::move(data));
  return true;
}



void CollisionOverlay::UnloadChunk(const std::string &chunkName) {
  for (size_t i = 0; i < m_chunks.size(); ++i) {
    if (m_chunks[i].chunkName == chunkName) {
      FreeCollisionBatches(m_chunks[i]);
      // O(1) removal using swap-and-pop instead of O(N) element shifting
      std::swap(m_chunks[i], m_chunks.back());
      m_chunks.pop_back();
      return;
    }
  }
}

Vector3 CollisionOverlay::WorldFromRaw(int32_t rawX, int32_t rawY,
                                        int32_t rawZ,
                                        const ParsedCollision &coll) {
  const float SCALE = 1.0f / 256.0f; // IPD_SCALE is 1/256
  return {(rawX + coll.positionX) * SCALE, -rawY * SCALE,
          -(rawZ + coll.positionZ) * SCALE};
}

// ---------------------------------------------------------------------------
// Mesh generation
// ---------------------------------------------------------------------------

void CollisionOverlay::BuildCollisionBatches(CollisionChunkData &outData,
                                              const ParsedCollision &coll) {
  // 1. Build Terrain (Walkable vs Non-Walkable)
  std::vector<Vector3> walkableVerts;
  std::vector<Vector3> nonWalkableVerts;

  for (int gz = 0; gz < coll.gridHeight; ++gz) {
    for (int gx = 0; gx < coll.gridWidth; ++gx) {
      int gi = gz * coll.gridWidth + gx;
      if (gi >= coll.grid.size())
        continue;

      int b5_start = std::max(0, (int)coll.grid[gi].start);
      int b5_end = (gi + 1 < (int)coll.grid.size())
                       ? std::max(b5_start, (int)coll.grid[gi + 1].start)
                       : (int)coll.block5.size();
      b5_end = std::min(b5_end, (int)coll.block5.size());

      int b6_start = std::max(0, (int)coll.grid[gi].end);
      int b6_end = (gi + 1 < (int)coll.grid.size())
                       ? std::max(b6_start, (int)coll.grid[gi + 1].end)
                       : (int)coll.block6.size();
      b6_end = std::min(b6_end, (int)coll.block6.size());

      std::vector<uint8_t> candidates;
      for (int i = b5_start; i < b5_end; ++i) {
        candidates.push_back(coll.block5[i]);
      }
      for (int i = b6_start; i < b6_end; ++i) {
        candidates.push_back(coll.block6[i]);
      }

      bool walkable = false;
      const ParsedCollision::Surface *activeSurface = nullptr;

      for (uint8_t subcellIdx : candidates) {
        if (subcellIdx < coll.subcells.size()) {
          const auto &sub = coll.subcells[subcellIdx];
          uint8_t sIdx[2] = {sub.surfaceIdx0, sub.surfaceIdx1};
          for (int j = 0; j < 2; ++j) {
            if (sIdx[j] != 255 && sIdx[j] < coll.surfaces.size()) {
              const auto &s = coll.surfaces[sIdx[j]];
              if ((s.tilt_flags & 0x1F) != 12 &&
                  ((s.tilt_flags >> 5) & 7) == 0) {
                walkable = true;
                activeSurface = &s;
                break;
              }
            }
          }
        }
        if (walkable)
          break;
      }

      // If not walkable, find any surface for height
      if (!walkable) {
        for (uint8_t subcellIdx : candidates) {
          if (subcellIdx < coll.subcells.size()) {
            const auto &sub = coll.subcells[subcellIdx];
            uint8_t sIdx[2] = {sub.surfaceIdx0, sub.surfaceIdx1};
            for (int j = 0; j < 2; ++j) {
              if (sIdx[j] != 255 && sIdx[j] < coll.surfaces.size()) {
                activeSurface = &coll.surfaces[sIdx[j]];
                break;
              }
            }
          }
          if (activeSurface)
            break;
        }
      }

      int32_t y = activeSurface ? activeSurface->baseGroundHeight : 0;

      float rx0 = gx * coll.gridScale;
      float rz0 = gz * coll.gridScale;
      float rx1 = (gx + 1) * coll.gridScale;
      float rz1 = (gz + 1) * coll.gridScale;

      Vector3 v0 = WorldFromRaw(rx0, y, rz0, coll);
      Vector3 v1 = WorldFromRaw(rx1, y, rz0, coll);
      Vector3 v2 = WorldFromRaw(rx1, y, rz1, coll);
      Vector3 v3 = WorldFromRaw(rx0, y, rz1, coll);

      // Quad definition (2 triangles)
      std::vector<Vector3> &targetList =
          walkable ? walkableVerts : nonWalkableVerts;
      targetList.push_back(v0);
      targetList.push_back(v1);
      targetList.push_back(v2);

      targetList.push_back(v0);
      targetList.push_back(v2);
      targetList.push_back(v3);
    }
  }

  auto uploadMesh = [](const std::vector<Vector3> &verts, bool cycleHue,
                       Color baseColor) -> CollisionBatch {
    CollisionBatch batch;
    if (verts.empty())
      return batch;

    batch.mesh.vertexCount = verts.size();
    batch.mesh.triangleCount = verts.size() / 3;
    batch.mesh.vertices = (float *)MemAlloc(verts.size() * 3 * sizeof(float));
    batch.mesh.colors =
        (unsigned char *)MemAlloc(verts.size() * 4 * sizeof(unsigned char));

    for (size_t i = 0; i < verts.size(); ++i) {
      batch.mesh.vertices[i * 3 + 0] = verts[i].x;
      batch.mesh.vertices[i * 3 + 1] = verts[i].y;
      batch.mesh.vertices[i * 3 + 2] = verts[i].z;

      Color c = baseColor;
      if (cycleHue) {
        // Hue based on world y (elevation). Since scale is small, multiply by a
        // scalar to wrap hue
        float hue = fmodf(fabsf(verts[i].y) * 400.0f, 360.0f);
        c = ColorFromHSV(hue, 0.6f, 0.9f);
      }
      batch.mesh.colors[i * 4 + 0] = c.r;
      batch.mesh.colors[i * 4 + 1] = c.g;
      batch.mesh.colors[i * 4 + 2] = c.b;
      batch.mesh.colors[i * 4 + 3] = c.a;
    }

    UploadMesh(&batch.mesh, false);
    batch.meshUploaded = true;

    batch.material = LoadMaterialDefault();
    batch.material.maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    return batch;
  };

  if (!walkableVerts.empty()) {
    outData.terrainBatches.push_back(uploadMesh(walkableVerts, true, WHITE));
  }
  if (!nonWalkableVerts.empty()) {
    outData.terrainBatches.push_back(uploadMesh(nonWalkableVerts, true, WHITE));
  }

  // 1.5. Store Split Vertices for rendering
  for (const auto &sv : coll.splitVertices) {
    outData.splitVertices.push_back(WorldFromRaw(sv.x, sv.y, sv.z, coll));
  }

  // 2. Build Walls
  enum WallFlags {
    WALL_NONE = 0,
    WALL_VOID = 1 << 0,
    WALL_CAMERA = 1 << 1,
    WALL_PHYSICAL = 1 << 2
  };

  // Use a map to accumulate wall flags for each edge
  std::map<std::pair<int, int>, uint8_t> wallEdges;

  for (const auto &sub : coll.subcells) {
    int a = sub.splitVertexIdx0;
    int b = sub.splitVertexIdx1;
    if (a != b && a < coll.splitVertices.size() &&
        b < coll.splitVertices.size()) {
      Vector3 v0 =
          WorldFromRaw(coll.splitVertices[a].x, coll.splitVertices[a].y,
                       coll.splitVertices[a].z, coll);
      Vector3 v1 =
          WorldFromRaw(coll.splitVertices[b].x, coll.splitVertices[b].y,
                       coll.splitVertices[b].z, coll);

      // Offset slightly to prevent Z-fighting with terrain
      v0.y += 0.05f;
      v1.y += 0.05f;

      // The ground type / sound ID is in the lowest 5 bits of the surface's
      // tilt_flags. We'll try to read it from surfaceIdx0, or surfaceIdx1 if 0
      // is invalid.
      int groundType = 0;
      if (sub.surfaceIdx0 < coll.surfaces.size()) {
        groundType = coll.surfaces[sub.surfaceIdx0].tilt_flags & 0x1F;
      } else if (sub.surfaceIdx1 < coll.surfaces.size()) {
        groundType = coll.surfaces[sub.surfaceIdx1].tilt_flags & 0x1F;
      }

      // Deterministic color based on the groundType
      float hue = fmodf((float)groundType * 47.0f, 360.0f);
      Color c = ColorFromHSV(hue, 0.85f, 0.95f);

      outData.floorLines.push_back({v0, v1, c});
    }

    uint8_t flags = WALL_NONE;
    uint8_t sIdx[2] = {sub.surfaceIdx0, sub.surfaceIdx1};

    for (int j = 0; j < 2; ++j) {
      if (sIdx[j] == 255) {
        flags |= WALL_VOID;
      } else if (sIdx[j] < coll.surfaces.size()) {
        const auto &s = coll.surfaces[sIdx[j]];
        if ((s.tilt_flags & 0x1F) == 12) {
          flags |= WALL_PHYSICAL;
        } else if (((s.tilt_flags >> 5) & 7) != 0) {
          flags |= WALL_CAMERA;
        }
      }
    }

    if (flags != WALL_NONE) {
      int a = sub.splitVertexIdx0;
      int b = sub.splitVertexIdx1;
      if (a != b && a < coll.splitVertices.size() &&
          b < coll.splitVertices.size()) {
        auto pair = std::make_pair(std::min(a, b), std::max(a, b));
        wallEdges[pair] |= flags; // Accumulate flags instead of overriding
      }
    }
  }

  std::vector<Vector3> physicalWallVerts;
  std::vector<Vector3> cameraWallVerts;
  std::vector<Vector3> voidWallVerts;
  std::vector<Vector3> physicalVoidWallVerts;

  const float HEIGHT_PHYSICAL = 2.5f;
  const float HEIGHT_CAMERA = 3.5f;
  const float HEIGHT_VOID = 1.5f;
  const float HEIGHT_PHYSICAL_VOID = 3.0f;

  for (const auto &edge : wallEdges) {
    const auto &va = coll.splitVertices[edge.first.first];
    const auto &vb = coll.splitVertices[edge.first.second];
    uint8_t flags = edge.second;

    Vector3 p3 = WorldFromRaw(va.x, va.y, va.z, coll);
    Vector3 p2 = WorldFromRaw(vb.x, vb.y, vb.z, coll);

    float ext = HEIGHT_PHYSICAL;
    std::vector<Vector3> *targetList = &physicalWallVerts;

    if ((flags & WALL_PHYSICAL) && (flags & WALL_VOID)) {
      ext = HEIGHT_PHYSICAL_VOID;
      targetList = &physicalVoidWallVerts;
    } else if (flags & WALL_PHYSICAL) {
      ext = HEIGHT_PHYSICAL;
      targetList = &physicalWallVerts;
    } else if (flags & WALL_CAMERA) {
      ext = HEIGHT_CAMERA;
      targetList = &cameraWallVerts;
    } else if (flags & WALL_VOID) {
      ext = HEIGHT_VOID;
      targetList = &voidWallVerts;
    }

    float bottom_y3 = 0.0f;
    float bottom_y2 = 0.0f;

    if (p3.y - bottom_y3 < ext) {
      bottom_y3 = p3.y - ext;
    }
    if (p2.y - bottom_y2 < ext) {
      bottom_y2 = p2.y - ext;
    }

    Vector3 p0 = {p3.x, bottom_y3, p3.z};
    Vector3 p1 = {p2.x, bottom_y2, p2.z};

    targetList->push_back(p0);
    targetList->push_back(p1);
    targetList->push_back(p2);

    targetList->push_back(p0);
    targetList->push_back(p2);
    targetList->push_back(p3);
  }

  if (!physicalWallVerts.empty()) {
    outData.wallBatches.push_back(
        uploadMesh(physicalWallVerts, false, {216, 30, 20, 180})); // Wall red
  }
  if (!cameraWallVerts.empty()) {
    outData.wallBatches.push_back(
        uploadMesh(cameraWallVerts, false, {30, 150, 216, 180})); // Camera blue
  }
  if (!voidWallVerts.empty()) {
    outData.wallBatches.push_back(
        uploadMesh(voidWallVerts, false, {150, 150, 150, 180})); // Void grey
  }
  if (!physicalVoidWallVerts.empty()) {
    outData.wallBatches.push_back(
        uploadMesh(physicalVoidWallVerts, false,
                   {216, 150, 30, 180})); // Physical+Void Orange
  }
}

void CollisionOverlay::FreeCollisionBatches(CollisionChunkData &chunkData) {
  chunkData.terrainBatches.clear();
  chunkData.wallBatches.clear();
  chunkData.visualBatches.clear();
}

// ---------------------------------------------------------------------------
// DrawScene — called by ViewportBase between BeginMode3D/EndMode3D
// ---------------------------------------------------------------------------

void CollisionOverlay::DrawOverlay(Viewport& vp) {
  static const Matrix identity = MatrixIdentity();

  // Disable backface culling and depth writing for translucent drawing
  // (optional, but Raylib drawing is simple) Raylib rlgl handles standard alpha
  // blending
  for (const auto &lc : m_chunks) {
    if (!lc.visible)
      continue;

    // Draw visual faces and wireframes first
    if (m_showVisualGeometry) {
      for (const auto &b : lc.visualBatches) {
        if (b.meshUploaded) {
          // Draw solid flat grey face
          DrawMesh(b.mesh, b.material, identity);

          // Draw black wireframe over the top
          rlEnableWireMode();
          Color originalColor = b.material.maps[MATERIAL_MAP_ALBEDO].color;
          b.material.maps[MATERIAL_MAP_ALBEDO].color = BLACK;
          DrawMesh(b.mesh, b.material, identity);
          b.material.maps[MATERIAL_MAP_ALBEDO].color = originalColor;
          rlDisableWireMode();
        }
      }
    }

    for (const auto &b : lc.terrainBatches) {
      if (b.meshUploaded) {
        // Draw solid face
        DrawMesh(b.mesh, b.material, identity);

        // Draw black wireframe over the top
        rlEnableWireMode();
        Color originalColor = b.material.maps[MATERIAL_MAP_ALBEDO].color;
        b.material.maps[MATERIAL_MAP_ALBEDO].color = BLACK;
        DrawMesh(b.mesh, b.material, identity);
        b.material.maps[MATERIAL_MAP_ALBEDO].color = originalColor;
        rlDisableWireMode();
      }
    }

    for (const auto &b : lc.wallBatches) {
      if (b.meshUploaded) {
        DrawMesh(b.mesh, b.material, identity);
      }
    }

    // Draw split vertices
    for (const auto &sv : lc.splitVertices) {
      DrawCube(sv, 0.2f, 0.2f, 0.2f, YELLOW); // Draw as visible yellow cubes
      DrawCubeWires(sv, 0.2f, 0.2f, 0.2f, BLACK);
    }

    // Draw floor/sound subcell lines
    if (!lc.floorLines.empty()) {
      rlBegin(RL_LINES);
      for (const auto &line : lc.floorLines) {
        rlColor4ub(line.c.r, line.c.g, line.c.b, 255);
        rlVertex3f(line.a.x, line.a.y, line.a.z);
        rlVertex3f(line.b.x, line.b.y, line.b.z);
      }
      rlEnd();
    }
  }
}
