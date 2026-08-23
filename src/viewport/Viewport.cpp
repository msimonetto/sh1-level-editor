#include "viewport/Viewport.h"
#include "viewport/Collision.h"
#include "viewport/Waypoints.h"
#include "core/Config.h"
#include "core/Textures.h"
#include "formats/IPDParse.h"
#include "extras/IconsFontAwesome6.h"
#include "raymath.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cfloat>
#include "imgui.h"

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

Viewport::Viewport() : ViewportBase(ICON_FA_VIDEO " Viewport") {
    ResetCamera();
}

Viewport::~Viewport() {
    UnloadAll();    // Triggers OnUnloadAll() in base class via shutdown, but safe to
                    // call here too
}

void Viewport::OnUnloadAll() {
    for (auto &c : m_chunks) {
        FreeGpuBatches(c);
    }
    m_chunks.clear();
}

// ---------------------------------------------------------------------------
// Chunk management
// ---------------------------------------------------------------------------

bool Viewport::LoadChunk(std::shared_ptr<ParsedChunk> parsedChunk,
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

void Viewport::UnloadChunk(const std::string &chunkName) {
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

void Viewport::BuildGpuBatches(LoadedChunk &lc,
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

    printf("[Viewport] Built %zu GPU batches for '%s'\n", lc.batches.size(),
            lc.data->chunkName.c_str());
}

void Viewport::FreeGpuBatches(LoadedChunk &lc) { lc.batches.clear(); }

// ---------------------------------------------------------------------------
// DrawScene, Context Menu, & Status Bar
// ---------------------------------------------------------------------------

void Viewport::Draw() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  bool visible = ImGui::Begin(m_panelName.c_str());
  m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  ImGui::PopStyleVar();

  if (!visible) {
    ImGui::End();
    return;
  }

  // Available space for the render target
  float statusBarHeight =
      ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
  ImVec2 avail = ImGui::GetContentRegionAvail();
  int w = (int)avail.x;
  int h = (int)(avail.y - statusBarHeight);
  if (h <= 0)
    h = 1;

  DrawViewportCanvas(w, h);

  // --- Status Bar ---
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

  if (ImGui::Button("Controls")) {
    ImGui::OpenPopup("ControlsPopup");
  }

  ImGui::SameLine();
  if (ImGui::Button("Reset Camera")) {
    ResetCamera();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reset 3D camera position and orientation");
  }

  ImGui::SameLine();
  ImGui::Checkbox("Legend", &m_showChunkLegend);

  char speedText[64];
  snprintf(speedText, sizeof(speedText), "Speed: %.2fx", m_moveSpeedMultiplier);

  float speedWidth = ImGui::CalcTextSize(speedText).x;
  float rightOffsetX =
      ImGui::GetWindowWidth() - speedWidth - ImGui::GetStyle().WindowPadding.x;

  if (rightOffsetX > ImGui::GetCursorPosX()) {
    ImGui::SameLine(rightOffsetX);
  } else {
    ImGui::SameLine();
  }

  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("%s", speedText);

  if (ImGui::BeginPopup("ControlsPopup")) {
    ImGui::Text("Viewport Controls");
    ImGui::Separator();
    ImGui::TextDisabled(
        "MMB        = Orbit Camera\nShift+MMB  = Pan Camera\nScroll     = Zoom "
        "In/Out\n[ / ]      = Adjust Move Speed\nWASD       = Horizontal "
        "Move\nSpace      = Move Up\nLeft Shift = Move Down");
    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();

  ImGui::End();
}

void Viewport::DrawContextMenu() {
    auto it = m_overlays.find(m_activeMode);
    if (it != m_overlays.end() && it->second) {
        it->second->DrawContextMenu();
    } else if (m_activeMode == ViewportMode::Scene) {
        if (ImGui::MenuItem("Cut")) {}
        if (ImGui::MenuItem("Copy")) {}
        if (ImGui::MenuItem("Paste")) {}
        if (ImGui::MenuItem("Duplicate")) {}
        if (ImGui::MenuItem("Delete")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Edit Mesh")) {}
        if (ImGui::BeginMenu("Mirror")) {
            if (ImGui::MenuItem("X")) {}
            if (ImGui::MenuItem("Y")) {}
            if (ImGui::MenuItem("Z")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Rotate")) {
            if (ImGui::MenuItem("+X (90)")) {}
            if (ImGui::MenuItem("-X (90)")) {}
            if (ImGui::MenuItem("+Y (90)")) {}
            if (ImGui::MenuItem("-Y (90)")) {}
            if (ImGui::MenuItem("+Z (90)")) {}
            if (ImGui::MenuItem("-Z (90)")) {}
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Move to Chunk")) {}
    }
}

void Viewport::DrawScene() {
    auto it = m_overlays.find(m_activeMode);
    if (it != m_overlays.end() && it->second) {
        it->second->DrawOverlay(*this);
    } else {
        // Draw normal scene geometry...
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

        }
    }
}

std::vector<ChunkLocation> Viewport::GetChunkLocations() const {
    std::vector<ChunkLocation> locs;
    locs.reserve(m_chunks.size());
    for (const auto &lc : m_chunks) {
        locs.push_back({lc.data->chunkName, lc.data->xPos, lc.data->yPos, lc.bounds});
    }
    
    return locs;
}

void Viewport::SetCameraTarget(Vector3 target, float distance, float elevation, float azimuth) {
    m_camera.target = target;
    m_distance = distance;
    m_elevation = elevation;
    m_azimuth = azimuth;
    UpdateCameraVectors();
}

void Viewport::RebuildChunkBatches(const std::string &chunkName,
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

void Viewport::HandlePicking(Ray ray) {
    auto it = m_overlays.find(m_activeMode);
    if (it != m_overlays.end() && it->second) {
        it->second->HandlePicking(*this, ray);
        return;
    }

    // Default picking...
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

void Viewport::HandleBoxPicking(Rectangle box) {
    auto it = m_overlays.find(m_activeMode);
    if (it != m_overlays.end() && it->second) {
        it->second->HandleBoxPicking(*this, box);
    }
}
