#include "panels/TextureMapPanel.h"
#include "viewport/Viewport.h"
#include "core/Config.h"
#include "core/FileDialog.h"
#include "core/FileManager.h"
#include "core/DependencyManager.h"
#include "formats/IPDParse.h"
#include "formats/IPDWrite.h"
#include "imgui_internal.h"
#include "extras/IconsFontAwesome6.h"
#include "core/ResourceFilter.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

void TextureMapPanel::SaveRecentTiles(const std::string &workspaceDir) {
  std::string path = workspaceDir + "/recent_tiles.json";
  std::ofstream out(path);
  if (!out.is_open())
    return;

  out << "[\n";
  for (size_t i = 0; i < m_recentTiles.size(); ++i) {
    const auto &t = m_recentTiles[i];
    out << "  {\n";
    out << "    \"texName\": \"" << t.texName << "\",\n";
    out << "    \"palette\": " << t.palette << ",\n";
    out << "    \"minU\": " << t.minU << ",\n";
    out << "    \"minV\": " << t.minV << ",\n";
    out << "    \"maxU\": " << t.maxU << ",\n";
    out << "    \"maxV\": " << t.maxV << ",\n";
    out << "    \"rotationSteps\": " << t.rotationSteps << ",\n";
    out << "    \"isPinned\": " << (t.isPinned ? "true" : "false") << "\n";
    out << "  }" << (i == m_recentTiles.size() - 1 ? "" : ",") << "\n";
  }
  out << "]\n";
}

void TextureMapPanel::LoadRecentTiles(const std::string &workspaceDir) {
  std::string path = workspaceDir + "/recent_tiles.json";
  std::ifstream in(path);
  if (!in.is_open())
    return;

  m_recentTiles.clear();
  std::string line;
  SelectedTile current;
  bool inObject = false;

  while (std::getline(in, line)) {
    if (line.find("{") != std::string::npos) {
      inObject = true;
      current = {0, 0, 0, 0, 0, "", 0};
    } else if (line.find("}") != std::string::npos && inObject) {
      if (std::find(m_recentTiles.begin(), m_recentTiles.end(), current) == m_recentTiles.end()) {
        m_recentTiles.push_back(current);
      }
      inObject = false;
    } else if (inObject) {
      size_t colon = line.find(":");
      if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        key.erase(std::remove_if(
                      key.begin(), key.end(),
                      [](char c) { return c == '"' || c == ' ' || c == '\t'; }),
                  key.end());
        val.erase(std::remove_if(val.begin(), val.end(),
                                 [](char c) {
                                   return c == '"' || c == ',' || c == ' ' ||
                                          c == '\t';
                                 }),
                  val.end());

        if (key == "texName")
          current.texName = val;
        else if (key == "palette")
          current.palette = std::stoi(val);
        else if (key == "minU")
          current.minU = std::stof(val);
        else if (key == "minV")
          current.minV = std::stof(val);
        else if (key == "maxU")
          current.maxU = std::stof(val);
        else if (key == "maxV")
          current.maxV = std::stof(val);
        else if (key == "rotationSteps")
          current.rotationSteps = std::stoi(val);
        else if (key == "isPinned")
          current.isPinned = (val == "true" || val == "1");
      }
    }
  }
  std::stable_partition(m_recentTiles.begin(), m_recentTiles.end(),
                        [](const SelectedTile &t) { return t.isPinned; });
}

void TextureMapPanel::PushRecentTile(const SelectedTile &tile) {
  auto it = std::find(m_recentTiles.begin(), m_recentTiles.end(), tile);
  bool wasPinned = false;
  if (it != m_recentTiles.end()) {
    wasPinned = it->isPinned;
    m_recentTiles.erase(it);
  }

  SelectedTile newTile = tile;
  newTile.isPinned = wasPinned;
  m_recentTiles.push_front(newTile);
  std::stable_partition(m_recentTiles.begin(), m_recentTiles.end(),
                        [](const SelectedTile &t) { return t.isPinned; });

  if (m_recentTiles.size() > m_maxRecentTiles) {
    for (auto rit = m_recentTiles.rbegin(); rit != m_recentTiles.rend();
         ++rit) {
      if (!rit->isPinned) {
        m_recentTiles.erase(std::next(rit).base());
        break;
      }
    }
  }
}

void TextureMapPanel::EnsureRecentTilesLoaded(const std::string &workspaceDir) {
  static bool loadedRecentTiles = false;
  if (!loadedRecentTiles) {
    LoadRecentTiles(workspaceDir);
    
    auto it = m_recentTiles.begin();
    while (it != m_recentTiles.end()) {
      std::string path = workspaceDir + "/TIM/" + it->texName + ".TIM";
      if (!std::filesystem::exists(path)) {
        it = m_recentTiles.erase(it);
      } else {
        ++it;
      }
    }
    SaveRecentTiles(workspaceDir);

    if (!m_recentTiles.empty()) {
      m_currentTile = m_recentTiles.front();
    }
    loadedRecentTiles = true;
  }
}

void TextureMapPanel::SyncSelectionState(
    LocalGeometryOverlay &localGeometryOverlay, FileManager &fileManager,
    Textures &activeTexture, int &currentPalette, RenderFace *&activeFace,
    RenderMesh *&activeMesh, std::string *&activeObjName) {
  activeFace = nullptr;
  activeMesh = nullptr;
  activeObjName = nullptr;

  if (!localGeometryOverlay.m_selectedChunk.empty()) {
    for (auto &lc : localGeometryOverlay.GetChunks()) {
      if (lc.data->chunkName == localGeometryOverlay.m_selectedChunk) {
        if (localGeometryOverlay.m_selectedObjectIdx >= 0 &&
            localGeometryOverlay.m_selectedObjectIdx < (int)lc.data->objects.size()) {
          auto &obj = lc.data->objects[localGeometryOverlay.m_selectedObjectIdx];
          if (localGeometryOverlay.m_selectedMeshIdx >= 0 &&
              localGeometryOverlay.m_selectedMeshIdx < (int)obj.meshes.size()) {
            auto &mesh = obj.meshes[localGeometryOverlay.m_selectedMeshIdx];
            activeMesh = &mesh;
            activeObjName = &obj.name;

            if (localGeometryOverlay.m_selectedFaceIdx >= 0 &&
                localGeometryOverlay.m_selectedFaceIdx < (int)mesh.faces.size()) {
              activeFace = &mesh.faces[localGeometryOverlay.m_selectedFaceIdx];
            }
          }
        }
      }
    }
  }

  if (localGeometryOverlay.m_selectedChunk != lastSelChunk ||
      localGeometryOverlay.m_selectedObjectIdx != lastSelObj ||
      localGeometryOverlay.m_selectedMeshIdx != lastSelMesh ||
      localGeometryOverlay.m_selectedFaceIdx != lastSelFace) {

    lastSelChunk = localGeometryOverlay.m_selectedChunk;
    lastSelObj = localGeometryOverlay.m_selectedObjectIdx;
    lastSelMesh = localGeometryOverlay.m_selectedMeshIdx;
    lastSelFace = localGeometryOverlay.m_selectedFaceIdx;

    if (activeFace && !activeFace->texName.empty()) {
      std::string path = fileManager.GetWorkspaceDir() + "/TIM/" +
                         activeFace->texName + ".TIM";
      if (path != Config::Get().LastTexturePath) {
        if (activeTexture.Load(path)) {
          Config::Get().LastTexturePath = path;
          Config::Get().Save();
          currentPalette = activeFace->paletteRow;
          activeTexture.ApplyPalette(currentPalette);
        }
      } else if (activeTexture.GetTexture().id != 0) {
        currentPalette = activeFace->paletteRow;
        activeTexture.ApplyPalette(currentPalette);
      }

      float minU = activeFace->uv[0][0];
      float maxU = activeFace->uv[0][0];
      float minV = activeFace->uv[0][1];
      float maxV = activeFace->uv[0][1];
      int nVerts = (activeFace->v[3] == 0xFF) ? 3 : 4;
      for (int i = 1; i < nVerts; ++i) {
        minU = std::min(minU, activeFace->uv[i][0]);
        maxU = std::max(maxU, activeFace->uv[i][0]);
        minV = std::min(minV, activeFace->uv[i][1]);
        maxV = std::max(maxV, activeFace->uv[i][1]);
      }

      m_currentTile.minU = minU;
      m_currentTile.minV = minV;
      m_currentTile.maxU = maxU;
      m_currentTile.maxV = maxV;
      m_currentTile.rotationSteps = 0;
      m_currentTile.texName = activeFace->texName;
      m_currentTile.palette = activeFace->paletteRow;

      PushRecentTile(m_currentTile);
      SaveRecentTiles(fileManager.GetWorkspaceDir());
    }
  }
}

void TextureMapPanel::RefreshAvailableTextures(
    FileManager &fileManager, DependencyManager &dependencyManager,
    LocalGeometryOverlay &localGeometryOverlay) {
  std::string currentWorkspaceDir = fileManager.GetWorkspaceDir();
  std::string currentAssetsDir = fileManager.GetAssetsDir();
  std::string currentPrefix = fileManager.GetSelectedPrefix();
  std::vector<std::string> currentSelChunks = fileManager.GetSelectedChunks();
  std::string currentSelChunk = localGeometryOverlay.m_selectedChunk;
  int currentSelObj = localGeometryOverlay.m_selectedObjectIdx;
  int currentSelMesh = localGeometryOverlay.m_selectedMeshIdx;

  bool needRefresh =
      (m_filterScope != lastFilterScope) ||
      (currentWorkspaceDir != lastWorkspaceDirForTex) ||
      (currentAssetsDir != lastAssetsDirForTex) ||
      (currentPrefix != lastSelectedPrefixForTex) ||
      (currentSelChunks != lastSelectedChunksForTex) ||
      (currentSelChunk != lastSelChunk) ||
      (currentSelObj != lastSelObj) ||
      (currentSelMesh != lastSelMesh) ||
      (ImGui::GetTime() - lastTexRefreshTime > 1.0);

  if (needRefresh) {
    lastFilterScope = m_filterScope;
    lastWorkspaceDirForTex = currentWorkspaceDir;
    lastAssetsDirForTex = currentAssetsDir;
    lastSelectedPrefixForTex = currentPrefix;
    lastSelectedChunksForTex = currentSelChunks;
    lastSelChunk = currentSelChunk;
    lastSelObj = currentSelObj;
    lastSelMesh = currentSelMesh;
    lastTexRefreshTime = ImGui::GetTime();

    cachedTextures.clear();

    switch (m_filterScope) {
    case TextureFilterScope::Assets:
      cachedTextures =
          ResourceFilter::GetAssetTextures(fileManager, currentPrefix);
      break;
    case TextureFilterScope::Workspace:
      cachedTextures =
          ResourceFilter::GetWorkspaceTextures(fileManager, currentPrefix);
      break;
    case TextureFilterScope::SelectedChunks:
      cachedTextures = ResourceFilter::GetSelectedChunksTextures(
          fileManager, dependencyManager, currentPrefix);
      break;
    case TextureFilterScope::CurrentChunk: {
      const ParsedChunk *chunkData = nullptr;
      for (const auto &lc : localGeometryOverlay.GetChunks()) {
        if (lc.data && lc.data->chunkName == currentSelChunk) {
          chunkData = lc.data.get();
          break;
        }
      }
      cachedTextures =
          ResourceFilter::GetChunkTextures(chunkData, currentPrefix);
      break;
    }
    case TextureFilterScope::CurrentMesh: {
      const ParsedChunk *chunkData = nullptr;
      for (const auto &lc : localGeometryOverlay.GetChunks()) {
        if (lc.data && lc.data->chunkName == currentSelChunk) {
          chunkData = lc.data.get();
          break;
        }
      }
      const RenderMesh *mesh = nullptr;
      const RenderObject *obj = nullptr;
      if (chunkData && currentSelObj >= 0 &&
          currentSelObj < (int)chunkData->objects.size()) {
        obj = &chunkData->objects[currentSelObj];
        if (currentSelMesh >= 0 && currentSelMesh < (int)obj->meshes.size()) {
          mesh = &obj->meshes[currentSelMesh];
        }
      }
      cachedTextures =
          ResourceFilter::GetMeshTextures(mesh, chunkData, obj, currentPrefix);
      break;
    }
    default:
      break;
    }
  }
}

void TextureMapPanel::ApplyFaceMutation(
    LocalGeometryOverlay &localGeometryOverlay, Viewport &sceneViewport,
    FileManager &fileManager, History &history, RenderFace *activeFace,
    RenderMesh *activeMesh, std::string *activeObjName,
    const std::function<void(RenderFace &, const ParsedChunk *, RenderObject &)> &fn,
    const std::string &desc) {
  std::set<std::tuple<std::string, int, int>> affectedMeshes;

  if (localGeometryOverlay.m_editMode == EditMode::Face &&
      !localGeometryOverlay.m_selectedFaces.empty()) {
    std::map<std::tuple<std::string, int, int>, std::vector<int>> meshFaces;
    for (const auto &sf : localGeometryOverlay.m_selectedFaces) {
      meshFaces[std::make_tuple(sf.chunkName, sf.objectIdx, sf.meshIdx)].push_back(
          sf.faceIdx);
    }
    for (const auto &[meshKey, faceIndices] : meshFaces) {
      const std::string &cName = std::get<0>(meshKey);
      int oIdx = std::get<1>(meshKey);
      int mIdx = std::get<2>(meshKey);
      const ParsedChunk *cd = nullptr;
      for (const auto &lc : localGeometryOverlay.GetChunks()) {
        if (lc.data->chunkName == cName) {
          cd = lc.data.get();
          break;
        }
      }
      if (cd && oIdx >= 0 && oIdx < (int)cd->objects.size()) {
        auto &obj = const_cast<RenderObject &>(cd->objects[oIdx]);
        if (mIdx >= 0 && mIdx < (int)obj.meshes.size()) {
          auto &mesh = obj.meshes[mIdx];
          RenderMesh snapBefore = mesh;
          bool changed = false;
          for (int fIdx : faceIndices) {
            if (fIdx >= 0 && fIdx < (int)mesh.faces.size()) {
              fn(mesh.faces[fIdx], cd, obj);
              changed = true;
            }
          }
          if (changed) {
            RenderMesh snapAfter = mesh;
            history.Push({cName, oIdx, mIdx, snapBefore, snapAfter, desc});
            affectedMeshes.insert(meshKey);
          }
        }
      }
    }
  } else if (activeFace && activeMesh && activeObjName) {
    const ParsedChunk *cd = nullptr;
    for (const auto &lc : localGeometryOverlay.GetChunks()) {
      if (lc.data->chunkName == localGeometryOverlay.m_selectedChunk) {
        cd = lc.data.get();
        break;
      }
    }
    if (cd && localGeometryOverlay.m_selectedObjectIdx >= 0 &&
        localGeometryOverlay.m_selectedObjectIdx < (int)cd->objects.size()) {
      auto &obj = const_cast<RenderObject &>(
          cd->objects[localGeometryOverlay.m_selectedObjectIdx]);
      RenderMesh snapBefore = *activeMesh;
      fn(*activeFace, cd, obj);
      RenderMesh snapAfter = *activeMesh;
      history.Push({localGeometryOverlay.m_selectedChunk,
                    localGeometryOverlay.m_selectedObjectIdx,
                    localGeometryOverlay.m_selectedMeshIdx, snapBefore,
                    snapAfter, desc});
      affectedMeshes.insert({localGeometryOverlay.m_selectedChunk,
                             localGeometryOverlay.m_selectedObjectIdx,
                             localGeometryOverlay.m_selectedMeshIdx});
    }
  }

  std::set<std::string> chunksToRebuild;
  for (const auto &mKey : affectedMeshes)
    chunksToRebuild.insert(std::get<0>(mKey));
  for (const auto &cName : chunksToRebuild) {
    sceneViewport.RebuildChunkBatches(cName, fileManager.GetWorkspaceDir());
    localGeometryOverlay.RebuildChunkBatches(cName, fileManager.GetWorkspaceDir());
  }
}

void TextureMapPanel::DrawTextureSelector(
    Textures &activeTexture, int &currentPalette, FileManager &fileManager,
    Viewport &sceneViewport, LocalGeometryOverlay &localGeometryOverlay,
    History &history, RenderFace *activeFace, RenderMesh *activeMesh,
    std::string *activeObjName) {

  const char *filterScopeNames[] = {
      "Assets",
      "Workspace",
      "Selected Chunks",
      "Current Chunk",
      "Current Mesh"
  };

  float filterBtnWidth = ImGui::GetFrameHeight();
  ImGui::SetNextItemWidth(filterBtnWidth);
  if (ImGui::BeginCombo("##TexFilterScope", ICON_FA_FILTER,
                        ImGuiComboFlags_NoArrowButton)) {
    for (int i = 0; i < static_cast<int>(TextureFilterScope::Count); ++i) {
      bool isSelected = (m_filterScope == static_cast<TextureFilterScope>(i));
      if (ImGui::Selectable(filterScopeNames[i], isSelected)) {
        m_filterScope = static_cast<TextureFilterScope>(i);
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Filter Textures: %s",
                      filterScopeNames[static_cast<int>(m_filterScope)]);
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  std::string currentTexName = "Select a face or texture...";
  if (!Config::Get().LastTexturePath.empty()) {
    currentTexName =
        std::filesystem::path(Config::Get().LastTexturePath).stem().string();
  }

  if (cachedTextures.empty()) {
    std::string placeholder;
    std::string currentPrefix = fileManager.GetSelectedPrefix();
    switch (m_filterScope) {
    case TextureFilterScope::Assets:
      placeholder = currentPrefix.empty()
                        ? "No textures found in assets"
                        : "No " + currentPrefix + " textures in assets";
      break;
    case TextureFilterScope::Workspace:
      placeholder = currentPrefix.empty()
                        ? "No textures found in workspace"
                        : "No " + currentPrefix + " textures in workspace";
      break;
    case TextureFilterScope::SelectedChunks:
      placeholder = "No textures in selected chunks";
      break;
    case TextureFilterScope::CurrentChunk:
      placeholder = localGeometryOverlay.m_selectedChunk.empty()
                        ? "No chunk selected in viewport"
                        : "No textures in chunk " +
                              localGeometryOverlay.m_selectedChunk;
      break;
    case TextureFilterScope::CurrentMesh:
      placeholder = (localGeometryOverlay.m_selectedMeshIdx < 0)
                        ? "No mesh selected in viewport"
                        : "No textures in current mesh";
      break;
    default:
      placeholder = "No textures found";
      break;
    }
    if (ImGui::BeginCombo("##TIM_Sel", placeholder.c_str())) {
      ImGui::EndCombo();
    }
  } else {
    if (ImGui::BeginCombo("##TIM_Sel", currentTexName.c_str())) {
      for (const auto &tex : cachedTextures) {
        if (ImGui::Selectable(tex.c_str())) {
          std::string path =
              ResourceFilter::ResolveTexturePath(fileManager, tex);
          if (!path.empty() && activeTexture.Load(path)) {
            Config::Get().LastTexturePath = path;
            Config::Get().Save();
            currentPalette = 0;
            activeTexture.ApplyPalette(currentPalette);

            m_currentTile.texName = tex;
            m_currentTile.palette = 0;

            ApplyFaceMutation(
                localGeometryOverlay, sceneViewport, fileManager, history,
                activeFace, activeMesh, activeObjName,
                [&](RenderFace &face, const ParsedChunk *cd, RenderObject &obj) {
                  face.texName = tex;
                  face.texNum = 0x7F;
                  if (cd) {
                    const auto &texList =
                        obj.isGlobal ? cd->globalTexNames : cd->localTexNames;
                    for (size_t i = 0; i < texList.size(); i++) {
                      if (texList[i] == tex) {
                        face.texNum = (uint8_t)i;
                        break;
                      }
                    }
                  }
                },
                "Change texture to " + tex);
          }
        }
      }
      ImGui::EndCombo();
    }
  }
}

void TextureMapPanel::DrawPaletteControls(
    Textures &activeTexture, int &currentPalette, FileManager &fileManager,
    Viewport &sceneViewport, LocalGeometryOverlay &localGeometryOverlay,
    History &history, RenderFace *activeFace, RenderMesh *activeMesh,
    std::string *activeObjName) {
  ImGui::Text("Size: %d x %d", activeTexture.GetWidth(), activeTexture.GetHeight());

  int numPalettes = (int)activeTexture.GetPalettes().size();
  if (numPalettes >= 1) {
    auto applyNewPalette = [&](int newPal) {
      if (numPalettes <= 0) return;
      newPal = (newPal % numPalettes + numPalettes) % numPalettes;
      if (newPal == currentPalette) return;
      currentPalette = newPal;
      activeTexture.ApplyPalette(currentPalette);

      ApplyFaceMutation(
          localGeometryOverlay, sceneViewport, fileManager, history,
          activeFace, activeMesh, activeObjName,
          [&](RenderFace &face, const ParsedChunk *, RenderObject &) {
            face.paletteRow = currentPalette;
          },
          "Change palette to row " + std::to_string(currentPalette));
    };

    ImGui::Text("CLUT Row:");
    ImGui::SameLine();

    float btnW = 22.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float comboW = ImGui::GetContentRegionAvail().x - (btnW * 2.0f + spacing * 2.0f);
    if (comboW < 80.0f) comboW = 80.0f;

    bool canStep = (numPalettes > 1);
    if (!canStep) ImGui::BeginDisabled();
    if (ImGui::ArrowButton("##PrevPal", ImGuiDir_Left)) {
      applyNewPalette(currentPalette - 1);
    }
    if (!canStep) ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::SetNextItemWidth(comboW);
    std::string comboPreview = std::to_string(currentPalette);
    if (!canStep) ImGui::BeginDisabled();
    if (ImGui::BeginCombo("##PaletteCombo", comboPreview.c_str(), ImGuiComboFlags_HeightLarge)) {
      for (int i = 0; i < numPalettes; ++i) {
        ImGui::PushID(i);
        bool isSelected = (currentPalette == i);
        ImVec2 itemPos = ImGui::GetCursorScreenPos();
        float itemH = 18.0f;
        float itemW = ImGui::GetContentRegionAvail().x;

        if (ImGui::Selectable("##PalItem", isSelected, 0, ImVec2(0, itemH))) {
          applyNewPalette(i);
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        std::string numLabel = std::to_string(i);
        float labelW = 24.0f;
        drawList->AddText(
            ImVec2(itemPos.x + 4.0f, itemPos.y + 1.0f),
            isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255),
            numLabel.c_str());

        const auto &rowPal = activeTexture.GetPalettes()[i];
        if (!rowPal.colors.empty()) {
          float barX = itemPos.x + labelW;
          float barW = itemW - labelW - 4.0f;
          float barH = itemH - 4.0f;
          float barY = itemPos.y + 2.0f;
          int numCols = (int)rowPal.colors.size();
          float sW = barW / (float)numCols;

          drawList->AddRectFilled(ImVec2(barX, barY),
                                  ImVec2(barX + barW, barY + barH),
                                  IM_COL32(20, 20, 20, 255));
          for (int c = 0; c < numCols; ++c) {
            const auto &col = rowPal.colors[c];
            ImVec2 cp0(barX + c * sW, barY);
            ImVec2 cp1(barX + (c + 1) * sW, barY + barH);
            if (col.a == 0 && col.r == 0 && col.g == 0 && col.b == 0) {
              drawList->AddRectFilled(cp0, cp1, IM_COL32(25, 25, 30, 255));
              drawList->AddLine(cp0, cp1, IM_COL32(70, 70, 80, 255));
            } else {
              drawList->AddRectFilled(cp0, cp1,
                                      IM_COL32(col.r, col.g, col.b, 255));
            }
          }
          drawList->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                            IM_COL32(60, 60, 60, 255));
        }

        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
    if (!canStep) ImGui::EndDisabled();

    ImGui::SameLine();

    if (!canStep) ImGui::BeginDisabled();
    if (ImGui::ArrowButton("##NextPal", ImGuiDir_Right)) {
      applyNewPalette(currentPalette + 1);
    }
    if (!canStep) ImGui::EndDisabled();
  }

  if (!activeTexture.GetPalettes().empty()) {
    int palIdx =
        std::clamp(currentPalette, 0, (int)activeTexture.GetPalettes().size() - 1);
    const auto &pal = activeTexture.GetPalettes()[palIdx];
    if (!pal.colors.empty()) {
      float availWidth = ImGui::GetContentRegionAvail().x;
      float barHeight = 16.0f;
      ImVec2 barPos = ImGui::GetCursorScreenPos();
      ImDrawList *drawList = ImGui::GetWindowDrawList();

      ImGui::InvisibleButton("##PaletteColorPreview",
                             ImVec2(availWidth, barHeight));
      bool isHovered = ImGui::IsItemHovered();
      ImVec2 mousePos = ImGui::GetMousePos();

      int numColors = (int)pal.colors.size();
      float swatchW = availWidth / (float)numColors;

      int hoveredColorIdx = -1;
      if (isHovered && mousePos.x >= barPos.x &&
          mousePos.x < barPos.x + availWidth) {
        hoveredColorIdx =
            std::clamp((int)((mousePos.x - barPos.x) / swatchW), 0,
                       numColors - 1);
      }

      drawList->AddRectFilled(barPos,
                              ImVec2(barPos.x + availWidth, barPos.y + barHeight),
                              IM_COL32(20, 20, 20, 255));

      for (int i = 0; i < numColors; ++i) {
        const auto &c = pal.colors[i];
        ImVec2 p0(barPos.x + i * swatchW, barPos.y);
        ImVec2 p1(barPos.x + (i + 1) * swatchW, barPos.y + barHeight);

        if (c.a == 0 && c.r == 0 && c.g == 0 && c.b == 0) {
          drawList->AddRectFilled(p0, p1, IM_COL32(25, 25, 30, 255));
          drawList->AddLine(p0, p1, IM_COL32(70, 70, 80, 255));
        } else {
          drawList->AddRectFilled(p0, p1, IM_COL32(c.r, c.g, c.b, 255));
        }
      }

      drawList->AddRect(barPos,
                        ImVec2(barPos.x + availWidth, barPos.y + barHeight),
                        IM_COL32(80, 80, 80, 255));

      if (hoveredColorIdx >= 0 && hoveredColorIdx < numColors) {
        const auto &hc = pal.colors[hoveredColorIdx];
        ImVec2 hp0(barPos.x + hoveredColorIdx * swatchW, barPos.y);
        ImVec2 hp1(barPos.x + (hoveredColorIdx + 1) * swatchW,
                   barPos.y + barHeight);
        drawList->AddRect(hp0, hp1, IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);

        ImGui::BeginTooltip();
        ImGui::Text("Color #%d / %d", hoveredColorIdx, numColors);
        ImGui::Text("RGB: (%d, %d, %d)", hc.r, hc.g, hc.b);
        ImGui::Text("Hex: #%02X%02X%02X", hc.r, hc.g, hc.b);
        if (hc.a == 0)
          ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                             "(Transparent STP)");
        ImGui::EndTooltip();
      }
    }
  }
}

void TextureMapPanel::DrawUVCanvas(
    Textures &activeTexture, int &currentPalette, FileManager &fileManager,
    Viewport &sceneViewport, LocalGeometryOverlay &localGeometryOverlay,
    History &history, RenderFace *activeFace, RenderMesh *activeMesh,
    std::string *activeObjName) {
  if (activeTexture.GetTexture().id == 0)
    return;

  ImGui::Separator();
  ImGui::Checkbox("Snap UVs to 32x32 Grid", &snapToGrid);

  ImVec2 minUV, maxUV;
  RenderMesh snapBefore;
  if (m_canvas.Draw(activeTexture, activeFace, activeMesh, snapToGrid,
                    snapBefore, minUV, maxUV)) {
      m_currentTile.minU = minUV.x;
      m_currentTile.minV = minUV.y;
      m_currentTile.maxU = maxUV.x;
      m_currentTile.maxV = maxUV.y;
      m_currentTile.rotationSteps = 0;
      if (activeFace) {
        m_currentTile.texName = activeFace->texName;
      }
      m_currentTile.palette = currentPalette;

      PushRecentTile(m_currentTile);
      SaveRecentTiles(fileManager.GetWorkspaceDir());

      if (activeMesh) {
        RenderMesh snapAfter = *activeMesh;
        history.Push({localGeometryOverlay.m_selectedChunk,
                      localGeometryOverlay.m_selectedObjectIdx,
                      localGeometryOverlay.m_selectedMeshIdx, snapBefore,
                      snapAfter, "Edit UVs"});
        sceneViewport.RebuildChunkBatches(localGeometryOverlay.m_selectedChunk,
                                          fileManager.GetWorkspaceDir());
        localGeometryOverlay.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            fileManager.GetWorkspaceDir());
      }
    }
}

void TextureMapPanel::DrawRecentTilesGrid(
    Textures &activeTexture, int &currentPalette, FileManager &fileManager) {
  ImGui::Separator();

  ImGui::Text("Recent Tiles Cache:");
  ImGui::SameLine();
  if (ImGui::Button("Clear Cache")) {
    m_recentTiles.erase(
        std::remove_if(m_recentTiles.begin(), m_recentTiles.end(),
                       [](const SelectedTile &t) { return !t.isPinned; }),
        m_recentTiles.end());
    SaveRecentTiles(fileManager.GetWorkspaceDir());
  }

  const int cols = 8;
  float availW = ImGui::GetContentRegionAvail().x;
  float tileSize = std::floor(availW / (float)cols);
  if (tileSize < 16.0f)
    tileSize = 16.0f;

  float scrollChildH = tileSize * 2.0f;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

  ImGui::BeginChild("RecentTilesGrid", ImVec2(0, scrollChildH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

  float childAvailW = ImGui::GetContentRegionAvail().x;
  float displayTileSize = std::floor(childAvailW / (float)cols);
  if (displayTileSize < 16.0f)
    displayTileSize = 16.0f;

  auto drawTileItem = [&](size_t idx) {
    if (idx >= m_recentTiles.size())
      return;
    auto &t = m_recentTiles[idx];
    ImGui::PushID((int)idx);

    Texture2D cachedTex = TextureCache::Get().Fetch(
        t.texName, t.palette, fileManager.GetWorkspaceDir());

    int ctw = cachedTex.width > 0 ? cachedTex.width : 256;
    int cth = cachedTex.height > 0 ? cachedTex.height : 256;
    float itemTw = (t.maxU - t.minU) * (float)ctw;
    float itemTh = (t.maxV - t.minV) * (float)cth;

    float displayW = displayTileSize;
    float displayH = displayTileSize;

    ImVec2 uv0(t.minU, t.minV);
    ImVec2 uv1(t.maxU, t.maxV);

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1(p0.x + displayW, p0.y + displayH);

    ImGui::InvisibleButton("##TileBtn", ImVec2(displayW, displayH));
    bool isHovered = ImGui::IsItemHovered();
    bool isClicked = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool isRightClicked =
        isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    ImDrawList *drawList = ImGui::GetWindowDrawList();

    if (cachedTex.id != 0) {
      drawList->AddImage((ImTextureID)(intptr_t)cachedTex.id, p0, p1, uv0, uv1,
                         IM_COL32(255, 255, 255, 255));
    } else {
      drawList->AddRectFilled(p0, p1, IM_COL32(40, 40, 48, 255));
      drawList->AddText(ImVec2(p0.x + 2, p0.y + 2),
                        IM_COL32(180, 180, 180, 255), t.texName.c_str());
    }

    bool isCurrent = (m_currentTile == t);

    if (isCurrent) {
      drawList->AddRect(p0, p1, IM_COL32(0, 220, 255, 255), 0.0f, 0, 2.0f);
    } else if (isHovered) {
      drawList->AddRect(p0, p1, IM_COL32(255, 255, 255, 180), 0.0f, 0, 1.0f);
    }

    if (t.isPinned) {
      float pinX = p1.x - 7.0f;
      float pinY = p0.y + 7.0f;

      drawList->AddCircleFilled(ImVec2(pinX, pinY), 5.5f,
                                IM_COL32(0, 0, 0, 190));
      drawList->AddCircle(ImVec2(pinX, pinY), 5.5f,
                          IM_COL32(255, 200, 0, 220), 0, 1.0f);
      drawList->AddCircleFilled(ImVec2(pinX + 1.0f, pinY - 1.5f), 2.0f,
                                IM_COL32(255, 220, 50, 255));
      drawList->AddLine(ImVec2(pinX + 0.5f, pinY - 0.5f),
                        ImVec2(pinX - 2.0f, pinY + 2.0f),
                        IM_COL32(230, 230, 230, 255), 1.5f);
    }

    if (isClicked) {
      m_currentTile = t;
      std::string expectedPath =
          fileManager.GetWorkspaceDir() + "/TIM/" + t.texName + ".TIM";
      bool loadedNew = false;
      if (Config::Get().LastTexturePath != expectedPath || activeTexture.GetTexture().id == 0) {
        if (activeTexture.Load(expectedPath)) {
          Config::Get().LastTexturePath = expectedPath;
          Config::Get().Save();
          loadedNew = true;
        }
      }
      if (currentPalette != t.palette || loadedNew) {
        currentPalette = t.palette;
        activeTexture.ApplyPalette(currentPalette);
      }
    }

    if (isRightClicked) {
      t.isPinned = !t.isPinned;
      std::stable_partition(
          m_recentTiles.begin(), m_recentTiles.end(),
          [](const SelectedTile &st) { return st.isPinned; });
      SaveRecentTiles(fileManager.GetWorkspaceDir());
    }

    if (isHovered) {
      ImGui::BeginTooltip();
      ImGui::Text("%s (CLUT %d)", t.texName.c_str(), t.palette);
      ImGui::Text("Size: %dx%d px", (int)std::round(itemTw),
                  (int)std::round(itemTh));
      if (t.isPinned) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                           "★ Pinned (Right-click to unpin)");
      } else {
        ImGui::TextDisabled("Right-click to pin");
      }
      ImGui::EndTooltip();
    }

    ImGui::PopID();
  };

  if (m_recentTiles.empty()) {
    ImGui::TextDisabled("No recent tiles in cache.");
  } else {
    for (size_t i = 0; i < m_recentTiles.size(); ++i) {
      if (i > 0 && (i % (size_t)cols) != 0) {
        ImGui::SameLine(0.0f, 0.0f);
      }
      drawTileItem(i);
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleVar(3);
}

void TextureMapPanel::Draw(Textures &activeTexture, int &currentPalette,
                           FileManager &fileManager,
                           DependencyManager &dependencyManager,
                           Viewport &sceneViewport,
                           LocalGeometryOverlay &localGeometryOverlay,
                           History &history) {
  EnsureRecentTilesLoaded(fileManager.GetWorkspaceDir());

  RenderFace *activeFace = nullptr;
  RenderMesh *activeMesh = nullptr;
  std::string *activeObjName = nullptr;

  SyncSelectionState(localGeometryOverlay, fileManager, activeTexture,
                     currentPalette, activeFace, activeMesh, activeObjName);

  RefreshAvailableTextures(fileManager, dependencyManager, localGeometryOverlay);

  ImGui::SetNextWindowSizeConstraints(ImVec2(350, 400),
                                      ImVec2(FLT_MAX, FLT_MAX));
  if (!ImGui::Begin(ICON_FA_PAINTBRUSH " Texture Map")) {
    ImGui::End();
    return;
  }

  DrawTextureSelector(activeTexture, currentPalette, fileManager, sceneViewport,
                      localGeometryOverlay, history, activeFace, activeMesh,
                      activeObjName);

  if (activeTexture.GetTexture().id != 0) {
    ImGui::Separator();
    DrawPaletteControls(activeTexture, currentPalette, fileManager, sceneViewport,
                        localGeometryOverlay, history, activeFace, activeMesh,
                        activeObjName);
    DrawUVCanvas(activeTexture, currentPalette, fileManager, sceneViewport,
                 localGeometryOverlay, history, activeFace, activeMesh,
                 activeObjName);
  } else {
    ImGui::Separator();
    ImGui::TextDisabled("No TIM texture file selected.");
  }

  DrawRecentTilesGrid(activeTexture, currentPalette, fileManager);

  ImGui::End();
}
