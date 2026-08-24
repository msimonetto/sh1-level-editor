#include "panels/ChunksPanel.h"
#include "core/FileManager.h"
#include "core/Config.h"
#include "core/History.h"
#include "core/Patcher.h"
#include "core/Dictionary.h"
#include "core/DependencyManager.h"
#include "imgui.h"
#include "extras/IconsFontAwesome6.h"
#include <algorithm>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// ChunksPanel::DrawGrid
// Renders the spatial chunk grid and selection controls.
// Mirrors the former FileManager::DrawGrid().
// ---------------------------------------------------------------------------

void ChunksPanel::DrawGrid(FileManager& mgr, Dictionary& dict) {
  if (mgr.m_parsedChunks.empty()) {
    return;
  }

  auto it = mgr.m_parsedChunks.find(mgr.m_selectedPrefix);
  if (it == mgr.m_parsedChunks.end() || it->second.empty())
    return;

  const auto &chunks = it->second;

  // Use true global bounding box for Silent Hill 1 (16x18 grid)
  int minX = -8, maxX = 7;
  int minZ = -8, maxZ = 9;

  int cols = maxX - minX + 1;
  int rows = maxZ - minZ + 1;

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 p = ImGui::GetCursorScreenPos();

  float buttonWidth = 35.0f;
  float spacing = ImGui::GetStyle().ItemSpacing.x;
  float availWidth = ImGui::GetContentRegionAvail().x;
  float cellSize = availWidth / cols;
  float gridWidth = cols * cellSize;
  float gridHeight = rows * cellSize;

  // Background (transparent to ImGui window bg)
  drawList->AddRectFilled(p, ImVec2(p.x + gridWidth, p.y + gridHeight),
                          IM_COL32(0, 0, 0, 0));

  std::string workspaceDir = mgr.GetWorkspaceDir();
  std::string overrideDir = mgr.GetOverrideDir();

  static std::map<std::string, std::pair<bool, bool>> fsCache;
  static double lastFsCacheUpdate = 0;
  if (ImGui::GetTime() - lastFsCacheUpdate > 1.0) {
    fsCache.clear();
    lastFsCacheUpdate = ImGui::GetTime();
  }

  auto ensureCached = [&](const std::string &name) {
    if (fsCache.find(name) == fsCache.end()) {
      fsCache[name].first =
          fs::exists(fs::path(workspaceDir) / "IPD" / (name + ".IPD"));
      fsCache[name].second =
          fs::exists(fs::path(overrideDir) / "BG" / (name + ".IPD"));
    }
  };

  auto isExtracted = [&](const std::string &name) {
    ensureCached(name);
    return fsCache[name].first;
  };
  auto isDeployed = [&](const std::string &name) {
    ensureCached(name);
    return fsCache[name].second;
  };

  ImVec2 mousePos = ImGui::GetIO().MousePos;
  bool isClicked = ImGui::IsMouseClicked(0);
  bool isDragging = ImGui::IsMouseDragging(0);

  int hoveredX = -999;
  int hoveredZ = -999;
  if (ImGui::IsWindowHovered()) {
    float dx = mousePos.x - p.x;
    float dy = mousePos.y - p.y;
    if (dx >= 0 && dx < gridWidth && dy >= 0 && dy < gridHeight) {
      int col = (int)(dx / cellSize);
      int row = (int)(dy / cellSize);
      hoveredX = minX + col;
      hoveredZ = maxZ - row;
    }
  }

  static bool isRectSelecting = false;
  static int shiftStartX = -999;
  static int shiftStartZ = -999;
  static bool shiftIsSelecting = true;
  static std::vector<std::string> preShiftSelection;

  bool isShiftHeld = ImGui::GetIO().KeyShift;

  if (isShiftHeld && isClicked && !isRectSelecting) {
    if (hoveredX != -999) {
      isRectSelecting = true;
      shiftStartX = hoveredX;
      shiftStartZ = hoveredZ;
      preShiftSelection = mgr.m_selectedChunks;
      shiftIsSelecting = true;
      for (const auto &c : chunks) {
        if (c.hasCoords && c.x == hoveredX && c.z == hoveredZ) {
          if (std::find(preShiftSelection.begin(), preShiftSelection.end(), c.name) != preShiftSelection.end()) {
            shiftIsSelecting = false;
          }
          break;
        }
      }
    }
  }

  if (isRectSelecting) {
    if (!isShiftHeld) {
      // Cancelled by releasing shift early
      isRectSelecting = false;
      mgr.m_selectedChunks = preShiftSelection;
    } else if (ImGui::IsMouseReleased(0)) {
      // Finalized by releasing left click
      isRectSelecting = false;
    } else {
      // Live update
      if (hoveredX != -999) {
        mgr.m_selectedChunks = preShiftSelection;
        int rMinX = std::min(shiftStartX, hoveredX);
        int rMaxX = std::max(shiftStartX, hoveredX);
        int rMinZ = std::min(shiftStartZ, hoveredZ);
        int rMaxZ = std::max(shiftStartZ, hoveredZ);
        for (const auto &c : chunks) {
          if (c.hasCoords && c.x >= rMinX && c.x <= rMaxX && c.z >= rMinZ && c.z <= rMaxZ) {
            auto it = std::find(mgr.m_selectedChunks.begin(), mgr.m_selectedChunks.end(), c.name);
            if (shiftIsSelecting) {
              if (it == mgr.m_selectedChunks.end()) mgr.m_selectedChunks.push_back(c.name);
            } else {
              if (it != mgr.m_selectedChunks.end()) mgr.m_selectedChunks.erase(it);
            }
          }
        }
      }
    }
  }

  for (int z = minZ; z <= maxZ; z++) {
    for (int x = minX; x <= maxX; x++) {
      int col = x - minX;
      int row = maxZ - z;

      float x1 = p.x + col * cellSize;
      float y1 = p.y + row * cellSize;
      float x2 = x1 + cellSize;
      float y2 = y1 + cellSize;

      bool found = false;
      ChunkInfo cinfo;
      for (const auto &c : chunks) {
        if (c.hasCoords && c.x == x && c.z == z) {
          cinfo = c;
          found = true;
          break;
        }
      }

      if (found) {
        bool isSelected =
            std::find(mgr.m_selectedChunks.begin(), mgr.m_selectedChunks.end(),
                      cinfo.name) != mgr.m_selectedChunks.end();
        bool isViewport =
            std::find(mgr.m_viewportChunks.begin(), mgr.m_viewportChunks.end(),
                      cinfo.name) != mgr.m_viewportChunks.end();
        bool extracted = isExtracted(cinfo.name);
        bool deployed = isDeployed(cinfo.name);

        ImU32 color;
        if (isSelected) {
          if (deployed)
            color = IM_COL32(40, 160, 80, 255);
          else if (extracted)
            color = IM_COL32(200, 150, 40, 255);
          else
            color = IM_COL32(80, 80, 80, 255);
        } else {
          if (deployed)
            color = IM_COL32(20, 80, 40, 255);
          else if (extracted)
            color = IM_COL32(100, 80, 20, 255);
          else
            color = IM_COL32(43, 43, 43, 255);
        }

        // Hover + Click logic
        if (ImGui::IsWindowHovered() && mousePos.x >= x1 && mousePos.x <= x2 && mousePos.y >= y1 &&
            mousePos.y <= y2) {
          // Hover highlight
          color = IM_COL32(200, 200, 200, 255);

          std::string tooltip = cinfo.name + " (X: " + std::to_string(x) +
                                ", Z: " + std::to_string(z) + ")";
          if (dict.ChunkAliases.find(cinfo.name) != dict.ChunkAliases.end()) {
            tooltip += "\nAlias: " + dict.ChunkAliases[cinfo.name];
          }
          ImGui::SetTooltip("%s", tooltip.c_str());

          if (ImGui::IsMouseClicked(1)) {
            mgr.m_lastClickedChunk = cinfo.name;
            if (dict.ChunkAliases.find(cinfo.name) != dict.ChunkAliases.end()) {
              strncpy(mgr.m_aliasBuffer, dict.ChunkAliases[cinfo.name].c_str(),
                      sizeof(mgr.m_aliasBuffer));
            } else {
              mgr.m_aliasBuffer[0] = '\0';
            }
            ImGui::OpenPopup("ChunkContextMenu");
          } else if (isClicked && !isShiftHeld) {
            mgr.m_lastClickedChunk = cinfo.name;
            if (dict.ChunkAliases.find(cinfo.name) != dict.ChunkAliases.end()) {
              strncpy(mgr.m_aliasBuffer, dict.ChunkAliases[cinfo.name].c_str(),
                      sizeof(mgr.m_aliasBuffer));
            } else {
              mgr.m_aliasBuffer[0] = '\0';
            }

            if (isSelected) {
              mgr.m_selectedChunks.erase(std::remove(mgr.m_selectedChunks.begin(),
                                                 mgr.m_selectedChunks.end(),
                                                 cinfo.name),
                                     mgr.m_selectedChunks.end());
              mgr.m_isDragSelecting = false;
            } else {
              mgr.m_selectedChunks.push_back(cinfo.name);
              mgr.m_isDragSelecting = true;
            }
          } else if (isDragging && !isShiftHeld) {
            if (mgr.m_isDragSelecting) {
              if (!isSelected)
                mgr.m_selectedChunks.push_back(cinfo.name);
            } else {
              if (isSelected)
                mgr.m_selectedChunks.erase(std::remove(mgr.m_selectedChunks.begin(),
                                                   mgr.m_selectedChunks.end(),
                                                   cinfo.name),
                                       mgr.m_selectedChunks.end());
            }
          }
        }

        drawList->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), color);

        if (isViewport) {
          drawList->AddRect(ImVec2(x1 + 1.5f, y1 + 1.5f),
                            ImVec2(x2 - 1.5f, y2 - 1.5f),
                            IM_COL32(255, 255, 255, 255), 0.0f, 0, 3.0f);
        }
      } else {
        drawList->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2),
                                IM_COL32(25, 25, 25, 255));
      }

      drawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2),
                        IM_COL32(60, 60, 60, 255));

      // X and Z axes (drawn on edges instead of bisecting)
      // z = 0 is X axis (Red), moved down by one chunk (to y2)
      if (z == 0)
        drawList->AddLine(ImVec2(x1, y2), ImVec2(x2, y2),
                          IM_COL32(200, 50, 50, 255), 2.0f);
      // x = 0 is Z axis (Blue)
      if (x == 0)
        drawList->AddLine(ImVec2(x1, y1), ImVec2(x1, y2),
                          IM_COL32(50, 50, 200, 255), 2.0f);
    }
  }

  ImGui::Dummy(ImVec2(gridWidth, gridHeight));

  // Legend
  ImGui::Text("Legend:");
  ImGui::SameLine();
  ImGui::ColorButton("Source",
                     ImVec4(43 / 255.f, 43 / 255.f, 43 / 255.f, 1.0f));
  ImGui::SameLine();
  ImGui::Text("Source");
  ImGui::SameLine();
  ImGui::ColorButton("Workspace",
                     ImVec4(100 / 255.f, 80 / 255.f, 20 / 255.f, 1.0f));
  ImGui::SameLine();
  ImGui::Text("Workspace");
  ImGui::SameLine();
  ImGui::ColorButton("Deployment",
                     ImVec4(20 / 255.f, 80 / 255.f, 40 / 255.f, 1.0f));
  ImGui::SameLine();
  ImGui::Text("Deployment");

  float selSpacing = ImGui::GetStyle().ItemSpacing.x;
  float selWidth = (ImGui::GetContentRegionAvail().x - selSpacing * 3) / 4.0f;
  
  if (ImGui::Button(ICON_FA_BORDER_ALL " All", ImVec2(selWidth, 0))) {
    for (const auto &c : chunks) {
      if (std::find(mgr.m_selectedChunks.begin(), mgr.m_selectedChunks.end(), c.name) ==
          mgr.m_selectedChunks.end()) {
        mgr.m_selectedChunks.push_back(c.name);
      }
    }
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select All");
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_BORDER_NONE " None", ImVec2(selWidth, 0))) {
    mgr.m_selectedChunks.clear();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select None");
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FOLDER " Workspace", ImVec2(selWidth, 0))) {
    mgr.m_selectedChunks.clear();
    for (const auto &c : chunks) {
      if (isExtracted(c.name))
        mgr.m_selectedChunks.push_back(c.name);
    }
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select Workspace");
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FILE_EXPORT " Deployment", ImVec2(selWidth, 0))) {
    mgr.m_selectedChunks.clear();
    for (const auto &c : chunks) {
      if (isDeployed(c.name))
        mgr.m_selectedChunks.push_back(c.name);
    }
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select Deployment");

  if (ImGui::BeginPopup("ChunkContextMenu")) {
    ImGui::Text("Alias (%s):", mgr.m_lastClickedChunk.c_str());
    ImGui::PushItemWidth(200.0f);
    ImGui::InputText("##AliasEditor", mgr.m_aliasBuffer, sizeof(mgr.m_aliasBuffer));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
      dict.ChunkAliases[mgr.m_lastClickedChunk] = mgr.m_aliasBuffer;
      dict.Save();
      ImGui::CloseCurrentPopup();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Add Chunk to Workspace")) {}
    if (ImGui::MenuItem("Add Chunk to Viewport")) {}
    if (ImGui::MenuItem("Add Chunk to Deployment")) {}
    if (ImGui::MenuItem("Remove Chunk from Workspace")) {}
    if (ImGui::MenuItem("Remove Chunk from Viewport")) {}
    if (ImGui::MenuItem("Remove Chunk from Deployment")) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Restore to Original")) {}
    if (ImGui::MenuItem("Restore Dependencies to Original")) {}
    ImGui::Separator();
    if (ImGui::MenuItem("View Properties")) {}
    if (ImGui::MenuItem("View Hex Data")) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Verify Integrity")) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Cut")) {}
    if (ImGui::MenuItem("Copy")) {}
    if (ImGui::MenuItem("Paste")) {}
    if (ImGui::MenuItem("Delete")) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Bulldoze")) {}
    ImGui::EndPopup();
  }
}

// ---------------------------------------------------------------------------
// ChunksPanel::Draw — main entry point, called each frame from main.cpp
// ---------------------------------------------------------------------------

void ChunksPanel::Draw(FileManager& mgr, Dictionary& dict, DependencyManager& depMgr, History* editHistory) {
  ImGui::Begin(ICON_FA_CUBES " Chunks");

  if (ImGui::CollapsingHeader("Prefix", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (mgr.m_parsedChunks.empty()) {
      ImGui::Text("No chunks scanned.");
    } else {
      ImGui::PushItemWidth(100.0f);
      if (ImGui::BeginCombo("##PrefixCombo", mgr.m_selectedPrefix.c_str())) {
        for (const auto &pair : mgr.m_parsedChunks) {
          bool isSelected = (mgr.m_selectedPrefix == pair.first);
          std::string label = pair.first;
          if (label.empty()) label = "##empty_prefix";
          if (ImGui::Selectable(label.c_str(), isSelected)) {
            mgr.m_selectedPrefix = pair.first;
            Config::Get().SelectedPrefix = pair.first;
            Config::Get().Save();
          }
          if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
      ImGui::PopItemWidth();

      ImGui::SameLine();
      std::string prefixLabel = mgr.m_selectedPrefix;
      if (dict.PrefixNames.find(mgr.m_selectedPrefix) != dict.PrefixNames.end()) {
        prefixLabel = dict.PrefixNames[mgr.m_selectedPrefix];
      }
      ImGui::Text("%s", prefixLabel.c_str());

      float avail = ImGui::GetContentRegionAvail().x;
      float spacing = ImGui::GetStyle().ItemSpacing.x;
      if (ImGui::Button(ICON_FA_PLUS " Add", ImVec2((avail - spacing) / 2, 0))) {
        // stub
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_FA_MINUS " Remove", ImVec2((avail - spacing) / 2, 0))) {
        // stub
      }
    }
  }

  // Selection
  std::vector<std::string> prevSelection = mgr.m_selectedChunks;

  if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawGrid(mgr, dict);
  }

  if (prevSelection != mgr.m_selectedChunks) {
    depMgr.LoadIPDDependencies(mgr.m_selectedPrefix, mgr.m_selectedChunks);
  }

  // Actions
  if (ImGui::CollapsingHeader("Actions", ImGuiTreeNodeFlags_DefaultOpen)) {    float fullWidth = ImGui::GetContentRegionAvail().x;
    float halfWidth = (fullWidth - ImGui::GetStyle().ItemSpacing.x) / 2.0f;

    ImGui::Text(ICON_FA_VIDEO " Viewport");
    if (ImGui::Button(ICON_FA_BORDER_ALL " Add Selection", ImVec2(halfWidth, 0))) {
      for (const auto &c : mgr.m_selectedChunks) {
        if (fs::exists(fs::path(mgr.GetWorkspaceDir()) / "IPD" / (c + ".IPD"))) {
          if (std::find(mgr.m_viewportChunks.begin(), mgr.m_viewportChunks.end(), c) == mgr.m_viewportChunks.end()) {
            mgr.m_viewportChunks.push_back(c);
          }
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_BORDER_NONE " Remove Selection", ImVec2(halfWidth, 0))) {
      for (const auto &c : mgr.m_selectedChunks) {
        mgr.m_viewportChunks.erase(std::remove(mgr.m_viewportChunks.begin(), mgr.m_viewportChunks.end(), c), mgr.m_viewportChunks.end());
      }
    }
    if (ImGui::Button(ICON_FA_ERASER " Clear Viewport", ImVec2(fullWidth, 0))) {
      mgr.m_viewportChunks.clear();
    }

    ImGui::Separator();
    ImGui::Text(ICON_FA_FOLDER " Workspace");

    bool assetsExtracted = false;
    std::string assetsDir = mgr.GetAssetsDir();
    if (fs::exists(assetsDir) && !fs::is_empty(assetsDir)) {
      assetsExtracted = true;
    }

    if (assetsExtracted) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_EYE_DROPPER " Pull Game Assets", ImVec2(fullWidth, 0))) {
      std::string bin = mgr.GetGameBinSource();
      std::string out = mgr.GetAssetsDir();
      std::string proj = Config::Get().ProjectDirectory;
      std::thread([&mgr, bin, out, proj]() {
        fs::path cwd = fs::current_path();
        fs::path p1 = cwd / "scripts" / "backend" / "extract_assets.py";
        fs::path p2 = cwd / ".." / "scripts" / "backend" / "extract_assets.py";
        std::string scriptPath = (fs::exists(p1) ? p1 : p2).string();
        mgr.Log("[EXTRACT] Executing python extract_assets.py for " + bin + "...");
        std::string cmd = "python \"" + scriptPath + "\" \"" + bin + "\" \"" + out + "\"";
        int ret = std::system(cmd.c_str());
        if (ret == 0) mgr.Log("[EXTRACT] Extraction from BIN complete.");
        else mgr.Log("[EXTRACT] Failed to extract from BIN.", true);
      }).detach();
    }
    if (assetsExtracted) ImGui::EndDisabled();

    if (ImGui::Button(ICON_FA_BORDER_ALL " Add Chunks to Workspace", ImVec2(fullWidth, 0))) {
      std::vector<std::string> chunks = mgr.m_selectedChunks;
      std::string comp = mgr.GetAssetsDir();
      std::string work = mgr.GetWorkspaceDir();
      std::string proj = Config::Get().ProjectDirectory;
      std::thread([&mgr, chunks, comp, work, proj]() {
        mgr.ExtractToWorkspace(chunks, comp, work, proj);
      }).detach();
    }

    if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Restore Chunks", ImVec2(fullWidth, 0))) {
      ImGui::OpenPopup("RestoreChunksPopup");
    }

    if (ImGui::Button(ICON_FA_BORDER_NONE " Remove Chunks##Workspace", ImVec2(halfWidth, 0))) {
      std::vector<std::string> chunks = mgr.m_selectedChunks;
      std::string work = mgr.GetWorkspaceDir();
      std::string over = mgr.GetOverrideDir();
      std::string proj = Config::Get().ProjectDirectory;
      std::thread([&mgr, chunks, work, over, proj]() {
        mgr.DeleteSelected("workspace", chunks, false, work, over, proj);
      }).detach();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ERASER " Clear Workspace", ImVec2(halfWidth, 0))) {
      ImGui::OpenPopup("ClearWorkspacePopup");
    }

    ImGui::Separator();
    ImGui::Text(ICON_FA_FILE_EXPORT " Deployment");

    if (ImGui::Button(ICON_FA_BORDER_ALL " Add Chunks to Deployment", ImVec2(fullWidth, 0))) {
      std::vector<std::string> chunks = mgr.m_selectedChunks;
      std::string work = mgr.GetWorkspaceDir();
      std::string over = mgr.GetOverrideDir();
      std::string proj = Config::Get().ProjectDirectory;
      std::thread([&mgr, chunks, work, over, proj]() {
        mgr.DeployToTarget(chunks, work, over, proj);
      }).detach();
    }

    if (ImGui::Button(ICON_FA_BORDER_NONE " Remove Chunks##Deployment", ImVec2(halfWidth, 0))) {
      std::vector<std::string> chunks = mgr.m_selectedChunks;
      std::string work = mgr.GetWorkspaceDir();
      std::string over = mgr.GetOverrideDir();
      std::string proj = Config::Get().ProjectDirectory;
      std::thread([&mgr, chunks, work, over, proj]() {
        mgr.DeleteSelected("deployment", chunks, false, work, over, proj);
      }).detach();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ERASER " Clear Deployment", ImVec2(halfWidth, 0))) {
      ImGui::OpenPopup("ClearDeploymentPopup");
    }

    // Popups
    if (ImGui::BeginPopupModal("RestoreChunksPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Are you sure you want to restore the selected chunks to original?");
      static bool restoreDeps = false;
      ImGui::Checkbox("Restore associated dependencies", &restoreDeps);
      ImGui::Separator();
      if (ImGui::Button("Yes, restore", ImVec2(120, 0))) {
        std::vector<std::string> chunks = mgr.m_selectedChunks;
        std::string work = mgr.GetWorkspaceDir();
        std::string assets = mgr.GetAssetsDir();
        std::thread([&mgr, chunks, work, assets]() {
          bool deps = restoreDeps;
          mgr.RevertSelected(chunks, deps, work, assets);
          std::lock_guard<std::mutex> lock(mgr.m_reloadMutex);
          for (const auto &c : chunks) {
            if (std::find(mgr.m_reloadChunks.begin(), mgr.m_reloadChunks.end(), c) == mgr.m_reloadChunks.end()) {
              mgr.m_reloadChunks.push_back(c);
            }
          }
        }).detach();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("ClearWorkspacePopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Are you sure you want to clear the entire workspace?\nThis will delete all modified IPDs, PLMs, and TIMs.");
      ImGui::Separator();
      if (ImGui::Button("Yes, clear workspace!", ImVec2(150, 0))) {
        std::string work = mgr.GetWorkspaceDir();
        std::string over = mgr.GetOverrideDir();
        std::string proj = Config::Get().ProjectDirectory;
        std::thread([&mgr, work, over, proj]() {
          mgr.ClearEntire("workspace", work, over, proj);
        }).detach();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SetItemDefaultFocus();
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("ClearDeploymentPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Are you sure you want to clear the entire deployment override folder?\nThis will delete all files deployed to the game.");
      ImGui::Separator();
      if (ImGui::Button("Yes, clear deployment!", ImVec2(160, 0))) {
        std::string work = mgr.GetWorkspaceDir();
        std::string over = mgr.GetOverrideDir();
        std::string proj = Config::Get().ProjectDirectory;
        std::thread([&mgr, work, over, proj]() {
          mgr.ClearEntire("deployment", work, over, proj);
        }).detach();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SetItemDefaultFocus();
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    ImGui::Separator();
    ImGui::Text(ICON_FA_FILE_EXPORT " Export");

    if (ImGui::Button(ICON_FA_BORDER_ALL " Export Selection as OBJ", ImVec2(fullWidth, 0))) {
      std::vector<std::string> chunks = mgr.m_selectedChunks;
      std::string comp = mgr.GetAssetsDir();
      std::string work = mgr.GetWorkspaceDir();
      std::string proj = Config::Get().ProjectDirectory;
      std::thread([&mgr, chunks, comp, work, proj]() {
        mgr.ExportToOBJ(chunks, work, comp, proj);
      }).detach();
    }

    ImGui::Separator();
    ImGui::Text(ICON_FA_SCREWDRIVER_WRENCH " Engine Patches");

    std::vector<std::string> needsPatchingChunks;
    bool patchRequired = EnginePatcher::CheckPatchingRequired(
        mgr.GetOverrideDir(), Config::Get().GameDirectory, mgr.m_patchVersion,
        &needsPatchingChunks);
    if (patchRequired) {
      std::string listStr = "";
      for (size_t k = 0; k < needsPatchingChunks.size(); ++k) {
        if (k > 0) listStr += ", ";
        listStr += needsPatchingChunks[k];
      }
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                         ICON_FA_TRIANGLE_EXCLAMATION " Patching required. File table size bounds exceeded (%s).",
                         listStr.c_str());
    } else {
      ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                         ICON_FA_CHECK " No file table size bounds changes detected.");
    }

    ImGui::PushItemWidth(100.0f);
    const char *patchVersions[] = {"USA", "EU", "JP", "ALL"};
    int currentVersion = (int)mgr.m_patchVersion;
    if (ImGui::Combo("Patch Version", &currentVersion, patchVersions, IM_ARRAYSIZE(patchVersions))) {
      mgr.m_patchVersion = (EnginePatcher::Version)currentVersion;
    }
    ImGui::PopItemWidth();

    if (ImGui::Button(ICON_FA_SCREWDRIVER_WRENCH " Patch File Tables", ImVec2(halfWidth, 0))) {
      std::string over = mgr.GetOverrideDir();
      std::string engine = Config::Get().GameDirectory;
      EnginePatcher::Version ver = mgr.m_patchVersion;
      std::thread([&mgr, over, engine, ver]() {
        mgr.Log("[PATCH] Patching File Tables...");
        bool ok = EnginePatcher::PatchMemoryAllocations(over, engine, ver);
        if (ok) mgr.Log("[PATCH] Patching File Tables complete.");
        else mgr.Log("[PATCH] Engine patching failed or no chunks required patching.", true);
      }).detach();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Restore File Tables", ImVec2(halfWidth, 0))) {
      std::string over = mgr.GetOverrideDir();
      std::string engine = Config::Get().GameDirectory;
      EnginePatcher::Version ver = mgr.m_patchVersion;
      std::thread([&mgr, over, engine, ver]() {
        mgr.Log("[PATCH] Reverting Engine Memory Allocations...");
        bool ok = EnginePatcher::RevertMemoryAllocations(over, engine, ver);
        if (ok) mgr.Log("[PATCH] Engine revert complete.");
        else mgr.Log("[PATCH] Engine revert failed or no backups found.", true);
      }).detach();
    }

    if (ImGui::Button(ICON_FA_CODE " Compile", ImVec2(fullWidth, 0))) {
      std::string buildDir = mgr.GetBuildDir();
      std::thread([&mgr, buildDir]() {
        mgr.Log("[BUILD] Recompiling Source in " + buildDir + "...");
        if (!fs::exists(buildDir)) {
          mgr.Log("[BUILD] Build directory does not exist.", true);
          return;
        }
        std::string cmd = "cd \"" + buildDir + "\" && ninja";
        int ret = std::system(cmd.c_str());
        if (ret == 0) mgr.Log("[BUILD] Recompile Source complete.");
        else mgr.Log("[BUILD] Failed to Recompile Source. Check terminal output.", true);
      }).detach();
    }
  }

  ImGui::End(); // End Chunks

  // Draw Pipeline Progress UI
  {
    std::lock_guard<std::mutex> lock(mgr.m_progressMutex);
    if (mgr.m_progressTotal > 0) {
      ImVec2 center = ImGui::GetMainViewport()->GetCenter();
      ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
      ImGuiWindowFlags flags =
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

      ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
      if (ImGui::Begin("PipelineProgress", nullptr, flags)) {
        ImGui::Text("%s", mgr.m_progressOp.c_str());
        ImGui::Spacing();
        float progress = ((float)mgr.m_progressCurrent / (float)mgr.m_progressTotal);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d / %d", mgr.m_progressCurrent,
                 mgr.m_progressTotal);
        ImGui::ProgressBar(progress, ImVec2(300.0f, 20.0f), buf);
        ImGui::End();
      }
      ImGui::PopStyleColor();

      if (mgr.m_progressCurrent >= mgr.m_progressTotal) {
        mgr.m_progressTotal = 0; // Reset once complete
      }
    }
  }

  // Console
  ImGui::Begin("Console");

  std::lock_guard<std::mutex> lock(mgr.m_consoleMutex);
  ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false,
                    ImGuiWindowFlags_HorizontalScrollbar);
  for (const auto &line : mgr.m_consoleLines) {
    if (line.find("[ERROR]") != std::string::npos) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
      ImGui::TextUnformatted(line.c_str());
      ImGui::PopStyleColor();
    } else if (line.find("[SAVE]") != std::string::npos) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 1.0f, 1.0f));
      ImGui::TextUnformatted(line.c_str());
      ImGui::PopStyleColor();
    } else {
      ImGui::TextUnformatted(line.c_str());
    }
  }
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);
  ImGui::EndChild();
  ImGui::End();
}
