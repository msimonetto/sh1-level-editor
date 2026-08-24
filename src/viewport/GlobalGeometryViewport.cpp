#include "viewport/GlobalGeometryViewport.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>

#include "formats/Structs.h"
#include "formats/PLMParse.h"
#include "imgui.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "extras/IconsFontAwesome6.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

GlobalGeometryViewport::GlobalGeometryViewport()
    : ViewportBase(ICON_FA_SHAPES " Global Geometry") {
  m_distance = 4.0f; // Closer default zoom
  m_azimuth = 45.0f;
  m_elevation = 25.0f;
}

GlobalGeometryViewport::~GlobalGeometryViewport() {
  ClearGpuBatches();
  if (m_depThread.joinable())
    m_depThread.join();
}

void GlobalGeometryViewport::SetWorkspaceDir(const std::string &dir) {
  if (m_workspaceDir != dir) {
    m_workspaceDir = dir;
    m_availableFiles.clear();
    std::string geomDir = m_workspaceDir + "/PLM";
    if (fs::exists(geomDir)) {
      for (const auto &entry : fs::directory_iterator(geomDir)) {
        if (entry.is_regular_file() && (entry.path().extension() == ".PLM" || entry.path().extension() == ".plm")) {
          m_availableFiles.push_back(entry.path().stem().string());
        }
      }
    }
    if (!m_availableFiles.empty()) {
        std::sort(m_availableFiles.begin(), m_availableFiles.end());
        m_currentFile = m_availableFiles[0];
    }
    AutoLoadPlmFile(m_currentFile);
  }
}

// ---------------------------------------------------------------------------
// Main 2-Column Panel Layout
// ---------------------------------------------------------------------------

void GlobalGeometryViewport::Draw() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  bool visible = ImGui::Begin(m_panelName.c_str());
  m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  ImGui::PopStyleVar();

  if (!visible) {
    ImGui::End();
    return;
  }

  ImVec2 avail = ImGui::GetContentRegionAvail();
  if (avail.x < 2.f || avail.y < 2.f) {
    ImGui::End();
    return;
  }

  // Split horizontally: 38% left management panel, 62% right 3D viewport canvas
  float leftWidth = avail.x * 0.38f;
  if (leftWidth < 280.f)
    leftWidth = 280.f;
  float rightWidth = avail.x - leftWidth - 4.f;

  DrawLeftPanel(leftWidth);

  ImGui::SameLine(0.f, 4.f);

  // Right Column: Render isolated 3D canvas (NO status bar at bottom!)
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::BeginChild("##gg_vp_container", ImVec2(rightWidth, avail.y), false,
                    ImGuiWindowFlags_NoScrollbar);

  // Lock camera target to Y axis center of current object (no panning / WASD
  // position translation)
  float yCenter = 0.0f;
  if (m_selectedIdx >= 0 && m_selectedIdx < (int)m_objects.size()) {
    yCenter = (m_objects[m_selectedIdx].bounds.min.y +
               m_objects[m_selectedIdx].bounds.max.y) *
              0.5f;
  }
  m_camera.target = {0.0f, yCenter, 0.0f};

  // Draw isolated 3D canvas without bottom status bar
  DrawViewportCanvas((int)rightWidth, (int)avail.y);

  ImGui::EndChild();
  ImGui::PopStyleVar();

  ImGui::End();
}

// ---------------------------------------------------------------------------
void GlobalGeometryViewport::DrawViewportGrid() {
  // Draw isolated 40x40 grid centered at origin (-20 to +20) instead of default
  // full chunk map grid
  DrawCustomGrid(20.0f);
}

void GlobalGeometryViewport::DrawScene() {
  if (m_selectedBatches.empty()) {
    // Wireframe cue if selected object has no batches built
    if (m_selectedIdx >= 0 && m_selectedIdx < (int)m_objects.size()) {
      const auto &robj = m_objects[m_selectedIdx].render_obj;
      for (const auto &mesh : robj.meshes) {
        for (size_t k = 0; k < mesh.vx.size(); ++k) {
          DrawPoint3D({mesh.vx[k], mesh.vy[k], mesh.vz[k]}, YELLOW);
        }
      }
    }
    return;
  }

  // Render textured GPU batches for the selected PLM object
  for (const auto &batch : m_selectedBatches) {
    if (!batch.meshUploaded)
      continue;
    DrawMesh(batch.mesh, batch.material, MatrixIdentity());
  }
}

// ===========================================================================
// Left Management Panel
// ===========================================================================

void GlobalGeometryViewport::DrawLeftPanel(float width) {
  ImGui::BeginChild("##gg_left", ImVec2(width, 0.f), true);

  // File Selector Header
  DrawFileSelector();

  if (!m_statusMsg.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.6f, 0.2f, 1.f));
    ImGui::TextWrapped("%s", m_statusMsg.c_str());
    ImGui::PopStyleColor();
  }

  ImGui::Separator();

  // Object List
  ImGui::TextUnformatted("Objects in Global Bank");
  DrawObjectList();

  ImGui::Separator();

  // Selected Details & Textures (Fix 5: Object-specific textures used)
  DrawSelectedDetails();

  ImGui::Separator();

  // Object-Specific Dependency Panel (Fix 7: Multi-directory scan + workspace
  // yellow label)
  DrawDependencyPanel();

  ImGui::Separator();

  // PLM File Size
  DrawCapacityBar();

  ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Prefix Selector
// ---------------------------------------------------------------------------

void GlobalGeometryViewport::DrawFileSelector() {
  ImGui::TextUnformatted("PLM File:");
  ImGui::SameLine();

  ImGui::PushItemWidth(100.f);
  if (ImGui::BeginCombo("##FileComboGG", m_currentFile.c_str())) {
    for (const auto &p : m_availableFiles) {
      bool isSel = (m_currentFile == p);
      if (ImGui::Selectable(p.c_str(), isSel)) {
        if (m_currentFile != p) {
          m_currentFile = p;
          AutoLoadPlmFile(p);
        }
      }
      if (isSel)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();

  ImGui::SameLine();
  if (ImGui::Button("Reload")) {
    AutoLoadPlmFile(m_currentFile);
  }

  if (!m_loadedGlbPath.empty()) {
    ImGui::TextDisabled("File: %s.PLM", m_currentFile.c_str());
  }
}

// ---------------------------------------------------------------------------
// Object List
// ---------------------------------------------------------------------------

void GlobalGeometryViewport::DrawObjectList() {
  ImVec2 listSize = ImVec2(0.f, 160.f);
  if (ImGui::BeginTable("##gg_objtbl", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingFixedFit,
                        listSize)) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Packs", ImGuiTableColumnFlags_WidthFixed, 46.f);
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < (int)m_objects.size(); ++i) {
      const auto &e = m_objects[i];
      ImGui::TableNextRow();

      if (e.is_dirty) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(90, 60, 10, 80));
      }

      ImGui::TableSetColumnIndex(0);
      char label[32];
      snprintf(label, sizeof(label), "%s%s##row%d", e.is_dirty ? "* " : "",
               e.name.c_str(), i);
      bool selected = (m_selectedIdx == i);
      if (ImGui::Selectable(label, selected,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        if (m_selectedIdx != i) {
          m_selectedIdx = i;
          RebuildGpuBatches(i);

          // Fix 8: Zoom closer to selected object based on bounds
          float minY = e.bounds.min.y;
          float maxY = e.bounds.max.y;
          float yCenter = (minY + maxY) * 0.5f;
          m_camera.target = {0.0f, yCenter, 0.0f};

          float dx = e.bounds.max.x - e.bounds.min.x;
          float dy = e.bounds.max.y - e.bounds.min.y;
          float dz = e.bounds.max.z - e.bounds.min.z;
          float maxDim = std::max({dx, dy, dz, 1.0f});

          m_distance = std::max(2.5f, std::min(12.0f, maxDim * 1.6f));
          UpdateCameraVectors();
        }
      }

      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", e.pack_count);

      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d", e.mesh_id);
    }
    ImGui::EndTable();
  }

  if (m_objects.empty()) {
    ImGui::TextDisabled("  No objects found for file '%s'.",
                        m_currentFile.c_str());
  }
}

// ---------------------------------------------------------------------------
// Selected Object Details & Textures (Fix 5: Object-specific textures used)
// ---------------------------------------------------------------------------

void GlobalGeometryViewport::DrawSelectedDetails() {
  if (m_selectedIdx < 0 || m_selectedIdx >= (int)m_objects.size()) {
    ImGui::TextDisabled("No object selected");
    return;
  }
  const auto &e = m_objects[m_selectedIdx];

  ImGui::Text("Active: %s", e.name.c_str());
  ImGui::Text("mesh_id: %d | Verts: %d | Packs: %d", e.mesh_id, e.vertex_count,
              e.pack_count);

  float kb = e.estimated_bytes / 1024.f;
  ImGui::Text("Est. Size: %.2f KB", kb);

  if (!e.used_textures.empty()) {
    ImGui::Spacing();
    ImGui::Text("Textures Used (%d):", (int)e.used_textures.size());

    if (ImGui::BeginTable("##gg_tex", 3,
                          ImGuiTableFlags_Borders |
                              ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0.f, 90.f))) {
      ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed,
                              32.f);
      ImGui::TableSetupColumn("Pal", ImGuiTableColumnFlags_WidthFixed, 36.f);
      ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      TextureCache &cache = TextureCache::Get();

      for (int t = 0; t < (int)e.used_textures.size(); ++t) {
        const auto &slot = e.used_textures[t];
        ImGui::TableNextRow();

        // Thumbnail
        ImGui::TableSetColumnIndex(0);
        Texture2D tex = cache.Fetch(slot.name, slot.palRow, m_workspaceDir);
        if (tex.id != 0) {
          ImGui::Image((ImTextureID)(intptr_t)tex.id, ImVec2(22.f, 22.f));
          if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s (Pal %d)", slot.name.c_str(), slot.palRow);
            ImGui::Image((ImTextureID)(intptr_t)tex.id, ImVec2(128.f, 128.f));
            ImGui::EndTooltip();
          }
        } else {
          ImGui::TextDisabled("[N/A]");
        }

        // Palette row index
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("P%d", slot.palRow);

        // Texture Name
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(slot.name.c_str());
      }
      ImGui::EndTable();
    }
  } else {
    ImGui::TextDisabled("  (no textures used by this object)");
  }
}

// ---------------------------------------------------------------------------
// Object-Specific Dependency Panel (Fix 7: Multi-directory scan + workspace
// yellow label)
// ---------------------------------------------------------------------------

void GlobalGeometryViewport::DrawDependencyPanel() {
  std::string selObjName =
      (m_selectedIdx >= 0 && m_selectedIdx < (int)m_objects.size())
          ? m_objects[m_selectedIdx].name
          : "";

  if (!selObjName.empty()) {
    ImGui::Text("Dependent Chunks (%s):", selObjName.c_str());
  } else {
    ImGui::TextUnformatted("Dependent Chunks:");
  }

  bool building = m_depIndexBuilding.load();
  if (building) {
    ImGui::SameLine();
    ImGui::TextDisabled("(scanning assets & workspace...)");
  }

  std::lock_guard<std::mutex> lock(m_depMutex);

  std::vector<ObjectChunkRef> activeDeps;
  if (!selObjName.empty() && m_objectDependencies.count(selObjName)) {
    activeDeps = m_objectDependencies[selObjName];
  }

  if (ImGui::BeginTable("##gg_dep", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_RowBg,
                        ImVec2(0.f, 85.f))) {
    ImGui::TableSetupColumn("Chunk", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Instances", ImGuiTableColumnFlags_WidthFixed,
                            65.f);
    ImGui::TableHeadersRow();

    for (const auto &ref : activeDeps) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);

      if (ref.isInWorkspace) {
        // Highlight yellow for workspace chunks
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.92f, 0.15f, 1.0f));
        ImGui::Text("%s", ref.chunkName.c_str());
        ImGui::PopStyleColor();
      } else {
        ImGui::TextUnformatted(ref.chunkName.c_str());
      }

      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d inst", ref.instanceCount);
    }
    ImGui::EndTable();
  }

  if (selObjName.empty()) {
    ImGui::TextDisabled("  Select an object to view its referencing chunks.");
  } else if (activeDeps.empty() && !building) {
    ImGui::TextDisabled("  No chunks reference %s.", selObjName.c_str());
  }
}

// ---------------------------------------------------------------------------
// File Capacity Bar
// ---------------------------------------------------------------------------

void GlobalGeometryViewport::DrawCapacityBar() {
  ImGui::TextUnformatted("Capacity:");

  int totalBytes = PLM_FILE_HEADER_SIZE;
  for (const auto &e : m_objects)
    totalBytes += e.estimated_bytes;

  float kb = totalBytes / 1024.f;
  float fraction = std::min(kb / CAPACITY_MAX_KB, 1.f);

  ImVec4 barColour;
  const char *tierLabel;
  if (kb < CAPACITY_WARN_KB) {
    barColour = ImVec4(0.2f, 0.7f, 0.3f, 1.f);
    tierLabel = "OK";
  } else if (kb < CAPACITY_MAX_KB) {
    barColour = ImVec4(0.9f, 0.65f, 0.1f, 1.f);
    tierLabel = "Near capacity.";
  } else {
    barColour = ImVec4(0.85f, 0.2f, 0.2f, 1.f);
    tierLabel = "Exceeded capacity! Remove objects to save PLM.";
  }

  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColour);
  char overlay[48];
  snprintf(overlay, sizeof(overlay), "%.1f KB  (%s)", kb, tierLabel);
  ImGui::ProgressBar(fraction, ImVec2(-1.f, 16.f), overlay);
  ImGui::PopStyleColor();
}

// ===========================================================================
// GPU Batch Building (Textured Mesh Rendering)
// ===========================================================================

void GlobalGeometryViewport::ClearGpuBatches() { m_selectedBatches.clear(); }

void GlobalGeometryViewport::RebuildGpuBatches(int idx) {
  ClearGpuBatches();

  if (idx < 0 || idx >= (int)m_objects.size())
    return;
  const auto &entry = m_objects[idx];
  const auto &robj = entry.render_obj;

  struct CpuBatch {
    std::string texName;
    uint8_t palRow;
    std::vector<float> positions;
    std::vector<float> texcoords;
    int vertexCount = 0;
  };
  std::map<std::string, CpuBatch> batchMap;

  for (const auto &mesh : robj.meshes) {
    for (const auto &face : mesh.faces) {
      std::string tName = face.texName;
      uint8_t pal = face.paletteRow;
      std::string key = tName + "_" + std::to_string(pal);

      auto &b = batchMap[key];
      b.texName = tName;
      b.palRow = pal;

      bool isQuad = (face.v[3] != 0xFF);
      int triCount = isQuad ? 2 : 1;
      static const int triV[2][3] = {{0, 1, 2}, {0, 2, 3}};

      for (int t = 0; t < triCount; ++t) {
        for (int c = 0; c < 3; ++c) {
          int vi = triV[t][c];
          int vIdx = face.v[vi];
          if (vIdx < (int)mesh.vx.size()) {
            b.positions.push_back(mesh.vx[vIdx]);
            b.positions.push_back(mesh.vy[vIdx]);
            b.positions.push_back(mesh.vz[vIdx]);
            b.texcoords.push_back(face.uv[vi][0]);
            b.texcoords.push_back(face.uv[vi][1]);
            b.vertexCount++;
          }
        }
      }
    }
  }

  TextureCache &cache = TextureCache::Get();

  for (auto &[key, cb] : batchMap) {
    if (cb.vertexCount == 0)
      continue;

    GpuBatch gpuBatch;
    gpuBatch.texName = cb.texName;
    gpuBatch.paletteRow = cb.palRow;

    Mesh mesh = {0};
    mesh.vertexCount = cb.vertexCount;
    mesh.triangleCount = cb.vertexCount / 3;

    mesh.vertices = (float *)RL_MALLOC(cb.positions.size() * sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(cb.texcoords.size() * sizeof(float));
    memcpy(mesh.vertices, cb.positions.data(),
           cb.positions.size() * sizeof(float));
    memcpy(mesh.texcoords, cb.texcoords.data(),
           cb.texcoords.size() * sizeof(float));

    UploadMesh(&mesh, false);
    gpuBatch.mesh = mesh;
    gpuBatch.meshUploaded = true;

    gpuBatch.material = cache.CreateMeshMaterial(cb.texName, cb.palRow, m_workspaceDir);

    m_selectedBatches.push_back(std::move(gpuBatch));
  }
}

// ===========================================================================
// Data Loading & Auto-Prefix Resolution
// ===========================================================================

void GlobalGeometryViewport::AutoLoadPlmFile(const std::string &filename) {
  m_statusMsg.clear();
  m_objects.clear();
  m_selectedIdx = -1;
  ClearGpuBatches();

  if (filename.empty()) {
      m_statusMsg = "No file selected.";
      return;
  }

  std::string glbPath = m_workspaceDir + "/PLM/" + filename + ".PLM";

  if (!fs::exists(glbPath)) {
    m_loadedGlbPath.clear();
    m_statusMsg = "No PLM found for '" + filename + "'.";
    return;
  }

  std::vector<RenderObject> renderObjs;
  std::vector<std::string> texNames;
  std::vector<IPDParse::GlbObjectInfo> info;

  if (!PLMParse::ParseGlbFile(glbPath, renderObjs, texNames, info)) {
    m_statusMsg = "Failed to parse PLM: " + glbPath;
    return;
  }

  m_loadedGlbPath = glbPath;

  m_objects.reserve(info.size());
  for (int i = 0; i < (int)info.size(); ++i) {
    PlmEntry e;
    e.name = info[i].name;
    e.mesh_id = info[i].mesh_id;
    e.pack_count = info[i].pack_count;
    e.render_obj = std::move(renderObjs[i]);

    // Fix 5: Extract unique (texName, palRow) pairs actually used by THIS
    // specific object
    std::set<std::pair<std::string, uint8_t>> uniqueTex;
    e.bounds = {{99999.f, 99999.f, 99999.f}, {-99999.f, -99999.f, -99999.f}};

    for (const auto &m : e.render_obj.meshes) {
      e.vertex_count += (int)m.vx.size();
      for (size_t k = 0; k < m.vx.size(); ++k) {
        e.bounds.min.x = std::min(e.bounds.min.x, m.vx[k]);
        e.bounds.min.y = std::min(e.bounds.min.y, m.vy[k]);
        e.bounds.min.z = std::min(e.bounds.min.z, m.vz[k]);
        e.bounds.max.x = std::max(e.bounds.max.x, m.vx[k]);
        e.bounds.max.y = std::max(e.bounds.max.y, m.vy[k]);
        e.bounds.max.z = std::max(e.bounds.max.z, m.vz[k]);
      }
      for (const auto &f : m.faces) {
        if (!f.texName.empty()) {
          uniqueTex.insert({f.texName, f.paletteRow});
        }
      }
    }

    for (const auto &[tname, pal] : uniqueTex) {
      e.used_textures.push_back({tname, pal});
    }

    e.estimated_bytes = EstimateObjectBytes(e);
    m_objects.push_back(std::move(e));
  }

  m_statusMsg = "";
  BuildDependencyIndex();
}

// ---------------------------------------------------------------------------
// Dependency Index Scanner (Fix 7: Scan assets + workspace, mark workspace
// yellow)
// ---------------------------------------------------------------------------

void GlobalGeometryViewport::BuildDependencyIndex() {
  if (m_depIndexBuilding.load())
    return;

  if (m_depThread.joinable())
    m_depThread.join();

  m_depIndexBuilding = true;
  std::string workDir = m_workspaceDir;

  m_depThread = std::thread([this, workDir]() {
    // Map: plmName -> Map: chunkName -> { count, isInWorkspace }
    struct TempRef {
      int count = 0;
      bool inWorkspace = false;
    };
    std::map<std::string, std::map<std::string, TempRef>> rawDeps;

    // Folders to scan
    std::vector<std::string> scanDirs = {
        workDir + "/IPD", workDir + "/../assets/BG", workDir + "/../assets"};

    std::set<std::string> scannedFiles;

    for (const auto &sDir : scanDirs) {
      if (!fs::exists(sDir))
        continue;

      for (const auto &entry : fs::directory_iterator(sDir)) {
        if (entry.is_regular_file() && (entry.path().extension() == ".IPD" ||
                                        entry.path().extension() == ".ipd")) {
          std::string chunkName = entry.path().stem().string();
          if (scannedFiles.count(chunkName))
            continue;
          scannedFiles.insert(chunkName);

          std::string filePath = entry.path().string();

          // Check if chunk binary exists in workspace
          bool inWorkspace =
              fs::exists(workDir + "/IPD/" + chunkName + ".IPD") ||
              fs::exists(workDir + "/IPD/" + chunkName + ".ipd");

          FILE *f = fopen(filePath.c_str(), "rb");
          if (!f)
            continue;
          fseek(f, 0, SEEK_END);
          long sz = ftell(f);
          fseek(f, 0, SEEK_SET);

          if (sz >= (long)sizeof(IPD_FILE_HEADER)) {
            std::vector<uint8_t> buf(sz);
            if (fread(buf.data(), 1, sz, f) == (size_t)sz) {
              const IPD_FILE_HEADER *hdr = (const IPD_FILE_HEADER *)buf.data();
              if (hdr->id == 0x14 && hdr->obj_name_offset > 0 &&
                  hdr->obj_num > 0) {
                int nameTableEnd =
                    hdr->obj_name_offset +
                    (int)hdr->obj_num * (int)sizeof(IPD_OBJNAME_DATA);
                if (nameTableEnd <= (int)buf.size()) {
                  std::map<int, std::string> globalObjNames;
                  for (int i = 0; i < hdr->obj_num; ++i) {
                    int off =
                        hdr->obj_name_offset + i * sizeof(IPD_OBJNAME_DATA);
                    const IPD_OBJNAME_DATA *nd =
                        (const IPD_OBJNAME_DATA *)(buf.data() + off);
                    if (nd->flag == 1) {
                      char nbuf[9] = {0};
                      memcpy(nbuf, nd->name, 8);
                      globalObjNames[i] = std::string(nbuf);
                    }
                  }

                  int posBase = hdr->obj_data_offset;
                  for (int p = 0; p < hdr->pos_num; ++p) {
                    int pOff = posBase + p * sizeof(IPD_POS_HEADER);
                    if (pOff + (int)sizeof(IPD_POS_HEADER) > (int)buf.size())
                      break;
                    const IPD_POS_HEADER *ph =
                        (const IPD_POS_HEADER *)(buf.data() + pOff);

                    int dOff = posBase + ph->data_offset;
                    for (int o = 0; o < ph->obj_num; ++o) {
                      int objOff = dOff + o * sizeof(IPD_OBJ_DATA);
                      if (objOff + (int)sizeof(IPD_OBJ_DATA) > (int)buf.size())
                        break;
                      const IPD_OBJ_DATA *od =
                          (const IPD_OBJ_DATA *)(buf.data() + objOff);

                      if (globalObjNames.count(od->obj_id)) {
                        std::string gName = globalObjNames[od->obj_id];
                        rawDeps[gName][chunkName].count++;
                        rawDeps[gName][chunkName].inWorkspace = inWorkspace;
                      }
                    }
                  }
                }
              }
            }
          }
          fclose(f);
        }
      }
    }

    std::map<std::string, std::vector<ObjectChunkRef>> finalDeps;
    for (const auto &[plmName, chunkMap] : rawDeps) {
      for (const auto &[cName, info] : chunkMap) {
        finalDeps[plmName].push_back({cName, info.count, info.inWorkspace});
      }
    }

    {
      std::lock_guard<std::mutex> lock(m_depMutex);
      m_objectDependencies = std::move(finalDeps);
    }
    m_depIndexBuilding = false;
  });
}

// ---------------------------------------------------------------------------
// Fast size estimation
// ---------------------------------------------------------------------------

int GlobalGeometryViewport::EstimateObjectBytes(const PlmEntry &e) {
  int total = PLM_OBJ_HEADER_SIZE;
  total += TEX_NAME_ENTRY_SIZE * (int)e.used_textures.size();
  for (const auto &mesh : e.render_obj.meshes) {
    total += PLM_DATA_HEADER_SIZE;
    total += (int)mesh.vx.size() * (PLM_VERTEX_XY_SIZE + PLM_VERTEX_Z_SIZE);
    total += (int)mesh.faces.size() * PLM_PACK_HEADER_SIZE;
  }
  total += 3;
  return total;
}

void GlobalGeometryViewport::DrawContextMenu() {
}

