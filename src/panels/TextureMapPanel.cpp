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
      m_recentTiles.push_back(current);
      inObject = false;
    } else if (inObject) {
      size_t colon = line.find(":");
      if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        // Cleanup key/val quotes and commas
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

void TextureMapPanel::Draw(Textures &testTexture, int &currentPalette,
                                FileManager &fileManager,
                                DependencyManager &dependencyManager,
                                Viewport &sceneViewport,
                                LocalGeometryOverlay &localGeometryOverlay,
                                History &history) {
  static bool loadedRecentTiles = false;
  if (!loadedRecentTiles) {
    LoadRecentTiles(fileManager.GetWorkspaceDir());
    
    auto it = m_recentTiles.begin();
    while (it != m_recentTiles.end()) {
        std::string path = fileManager.GetWorkspaceDir() + "/TIM/" + it->texName + ".TIM";
        if (!std::filesystem::exists(path)) {
            it = m_recentTiles.erase(it);
        } else {
            ++it;
        }
    }
    SaveRecentTiles(fileManager.GetWorkspaceDir());

    if (!m_recentTiles.empty()) {
      m_currentTile = m_recentTiles.front();
    }
    loadedRecentTiles = true;
  }

  // Sync texture manager with face selection

  RenderFace *activeFace = nullptr;
  RenderMesh *activeMesh = nullptr;
  std::string *activeObjName = nullptr;

  // Find active face/mesh if valid
  if (localGeometryOverlay.m_selectedChunk != "") {
    for (auto &lc : localGeometryOverlay.GetChunks()) {
      if (lc.data->chunkName == localGeometryOverlay.m_selectedChunk) {
        if (localGeometryOverlay.m_selectedObjectIdx >= 0 &&
            localGeometryOverlay.m_selectedObjectIdx <
                lc.data->objects.size()) {
          auto &obj =
              lc.data->objects[localGeometryOverlay.m_selectedObjectIdx];
          if (localGeometryOverlay.m_selectedMeshIdx >= 0 &&
              localGeometryOverlay.m_selectedMeshIdx < obj.meshes.size()) {
            auto &mesh = obj.meshes[localGeometryOverlay.m_selectedMeshIdx];
            activeMesh = &mesh;
            activeObjName = &obj.name;

            if (localGeometryOverlay.m_selectedFaceIdx >= 0 &&
                localGeometryOverlay.m_selectedFaceIdx < mesh.faces.size()) {
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
    
    static std::string lastFailedPath = "";
    lastFailedPath = "";

    // Backup face for reset functionality
    if (activeFace) {
      originalMeshBackup = *activeMesh;
      hasBackup = true;
    } else {
      hasBackup = false;
    }

    // if we selected a face, update texture manager
    if (activeFace && !activeFace->texName.empty()) {
      std::string path = fileManager.GetWorkspaceDir() + "/TIM/" +
                         activeFace->texName + ".TIM";
      if (path != Config::Get().LastTexturePath) {
        if (path != lastFailedPath) {
          if (testTexture.Load(path)) {
            Config::Get().LastTexturePath = path;
            Config::Get().Save();
            lastFailedPath = "";
            currentPalette = activeFace->paletteRow;
            testTexture.ApplyPalette(currentPalette);
          } else {
            lastFailedPath = path;
          }
        }
      } else if (testTexture.GetTexture().id != 0) {
        currentPalette = activeFace->paletteRow;
        testTexture.ApplyPalette(currentPalette);
      }

      // Automatically add selected face tile to recent tiles cache
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

  float labelWidth = 110.0f;
  float browseWidth = 80.0f;

  std::vector<std::string> selectedChunks = fileManager.GetSelectedChunks();
  if (selectedChunks.empty()) {
    selectedChunks = fileManager.GetViewportChunks();
  }
  if (selectedChunks.empty()) {
    if (!localGeometryOverlay.m_selectedChunk.empty()) {
      selectedChunks.push_back(localGeometryOverlay.m_selectedChunk);
    } else if (!sceneViewport.m_selectedChunk.empty()) {
      selectedChunks.push_back(sceneViewport.m_selectedChunk);
    }
  }

  ImGui::SetNextWindowSizeConstraints(ImVec2(350, 400), ImVec2(FLT_MAX, FLT_MAX));
  ImGui::Begin(ICON_FA_PAINTBRUSH " Texture Map");

  float totalAvailWidth = ImGui::GetContentRegionAvail().x;

  ImGui::AlignTextToFramePadding();
  ImGui::Text("From workspace:");
  ImGui::SameLine(labelWidth);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

  std::string currentWorkspaceDir = fileManager.GetWorkspaceDir();
  std::string currentPrefix = fileManager.GetSelectedPrefix();
  if (currentWorkspaceDir != lastWorkspaceDirForTex || currentPrefix != lastSelectedPrefixForTex || ImGui::GetTime() - lastTexRefreshTime > 1.0) {
    lastWorkspaceDirForTex = currentWorkspaceDir;
    lastSelectedPrefixForTex = currentPrefix;
    lastTexRefreshTime = ImGui::GetTime();
    cachedTextures.clear();
    
    std::string texDir = currentWorkspaceDir + "/TIM/";
    if (std::filesystem::exists(texDir) && std::filesystem::is_directory(texDir)) {
      for (const auto &entry : std::filesystem::directory_iterator(texDir)) {
        if (entry.is_regular_file()) {
          std::string ext = entry.path().extension().string();
          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
          if (ext == ".tim") {
            std::string stem = entry.path().stem().string();
            if (currentPrefix.empty() || stem.find(currentPrefix) == 0) {
              cachedTextures.push_back(stem);
            }
          }
        }
      }
      std::sort(cachedTextures.begin(), cachedTextures.end());
    }
  }

  auto applyToSelectedFaces = [&](auto fn, const std::string &desc) {
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
        if (cd && oIdx >= 0 && oIdx < cd->objects.size()) {
          auto &obj = const_cast<RenderObject &>(cd->objects[oIdx]);
          if (mIdx >= 0 && mIdx < obj.meshes.size()) {
            auto &mesh = obj.meshes[mIdx];
            RenderMesh snapBefore = mesh;
            bool changed = false;
            for (int fIdx : faceIndices) {
              if (fIdx >= 0 && fIdx < mesh.faces.size()) {
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
          localGeometryOverlay.m_selectedObjectIdx < cd->objects.size()) {
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
      sceneViewport.RebuildChunkBatches(cName,
                                        fileManager.GetWorkspaceDir());
      localGeometryOverlay.RebuildChunkBatches(
          cName, fileManager.GetWorkspaceDir());
    }
  };

  std::string currentTexName = "Select a texture...";
  if (!Config::Get().LastTexturePath.empty()) {
    currentTexName =
        std::filesystem::path(Config::Get().LastTexturePath).stem().string();
  }

  if (cachedTextures.empty()) {
    if (ImGui::BeginCombo("##TIM_Sel", "No textures found in workspace")) {
      ImGui::EndCombo();
    }
  } else {
    if (ImGui::BeginCombo("##TIM_Sel", currentTexName.c_str())) {
      for (const auto &tex : cachedTextures) {
        if (ImGui::Selectable(tex.c_str())) {
          std::string path =
              fileManager.GetWorkspaceDir() + "/TIM/" + tex + ".TIM";
          if (testTexture.Load(path)) {
            Config::Get().LastTexturePath = path;
            Config::Get().Save();
            currentPalette = 0;

            if (activeFace) {
              RenderMesh snapBefore = *activeMesh;
              activeFace->texName = tex;

              activeFace->texNum = 0x7F;
              const ParsedChunk *cd = nullptr;
              for (const auto &lc : localGeometryOverlay.GetChunks())
                if (lc.data->chunkName ==
                    localGeometryOverlay.m_selectedChunk) {
                  cd = lc.data.get();
                  break;
                }
              if (cd && localGeometryOverlay.m_selectedObjectIdx >= 0 &&
                  localGeometryOverlay.m_selectedObjectIdx <
                      cd->objects.size()) {
                const auto &texList =
                    cd->objects[localGeometryOverlay.m_selectedObjectIdx]
                            .isGlobal
                        ? cd->globalTexNames
                        : cd->localTexNames;
                for (size_t i = 0; i < texList.size(); i++) {
                  if (texList[i] == tex) {
                    activeFace->texNum = (uint8_t)i;
                    break;
                  }
                }
              }

              RenderMesh snapAfter = *activeMesh;
              history.Push({localGeometryOverlay.m_selectedChunk,
                            localGeometryOverlay.m_selectedObjectIdx,
                            localGeometryOverlay.m_selectedMeshIdx, snapBefore,
                            snapAfter, "Change texture to " + tex});
              sceneViewport.RebuildChunkBatches(
                  localGeometryOverlay.m_selectedChunk,
                  fileManager.GetWorkspaceDir());
              localGeometryOverlay.RebuildChunkBatches(
                  localGeometryOverlay.m_selectedChunk,
                  fileManager.GetWorkspaceDir());
            }
          }
        }
      }
      ImGui::EndCombo();
    }
  }

  ImGui::AlignTextToFramePadding();
  ImGui::Text("From file:");
  ImGui::SameLine(labelWidth);
  char timPathBuf[256];
  strncpy(timPathBuf, Config::Get().LastTexturePath.c_str(),
          sizeof(timPathBuf));

  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth -
                          ImGui::GetStyle().ItemSpacing.x);
  if (ImGui::InputText("##TIM_Path", timPathBuf, sizeof(timPathBuf),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    Config::Get().LastTexturePath = timPathBuf;
    Config::Get().Save();
    if (testTexture.Load(Config::Get().LastTexturePath)) {
      currentPalette = 0;

      if (activeFace) {
        RenderMesh snapBefore = *activeMesh;
        std::string tex = std::filesystem::path(Config::Get().LastTexturePath)
                              .stem()
                              .string();
        activeFace->texName = tex;

        activeFace->texNum = 0x7F;
        const ParsedChunk *cd = nullptr;
        for (const auto &lc : localGeometryOverlay.GetChunks())
          if (lc.data->chunkName == localGeometryOverlay.m_selectedChunk) {
            cd = lc.data.get();
            break;
          }
        if (cd && localGeometryOverlay.m_selectedObjectIdx >= 0 &&
            localGeometryOverlay.m_selectedObjectIdx < cd->objects.size()) {
          const auto &texList =
              cd->objects[localGeometryOverlay.m_selectedObjectIdx].isGlobal
                  ? cd->globalTexNames
                  : cd->localTexNames;
          for (size_t i = 0; i < texList.size(); i++) {
            if (texList[i] == tex) {
              activeFace->texNum = (uint8_t)i;
              break;
            }
          }
        }

        RenderMesh snapAfter = *activeMesh;
        history.Push({localGeometryOverlay.m_selectedChunk,
                      localGeometryOverlay.m_selectedObjectIdx,
                      localGeometryOverlay.m_selectedMeshIdx, snapBefore,
                      snapAfter, "Change texture to " + tex});
        sceneViewport.RebuildChunkBatches(localGeometryOverlay.m_selectedChunk,
                                          fileManager.GetWorkspaceDir());
        localGeometryOverlay.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            fileManager.GetWorkspaceDir());
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Browse...", ImVec2(browseWidth, 0))) {
    std::string path =
        FileDialog::OpenFile("TIM Files\0*.TIM;*.tim\0All Files\0*.*\0");
    if (!path.empty()) {
      Config::Get().LastTexturePath = path;
      Config::Get().Save();
      if (testTexture.Load(Config::Get().LastTexturePath)) {
        currentPalette = 0;

        if (activeFace) {
          RenderMesh snapBefore = *activeMesh;
          std::string tex = std::filesystem::path(Config::Get().LastTexturePath)
                                .stem()
                                .string();
          activeFace->texName = tex;

          activeFace->texNum = 0x7F;
          const ParsedChunk *cd = nullptr;
          for (const auto &lc : localGeometryOverlay.GetChunks())
            if (lc.data->chunkName == localGeometryOverlay.m_selectedChunk) {
              cd = lc.data.get();
              break;
            }
          if (cd && localGeometryOverlay.m_selectedObjectIdx >= 0 &&
              localGeometryOverlay.m_selectedObjectIdx < cd->objects.size()) {
            const auto &texList =
                cd->objects[localGeometryOverlay.m_selectedObjectIdx].isGlobal
                    ? cd->globalTexNames
                    : cd->localTexNames;
            for (size_t i = 0; i < texList.size(); i++) {
              if (texList[i] == tex) {
                activeFace->texNum = (uint8_t)i;
                break;
              }
            }
          }

          RenderMesh snapAfter = *activeMesh;
          history.Push({localGeometryOverlay.m_selectedChunk,
                        localGeometryOverlay.m_selectedObjectIdx,
                        localGeometryOverlay.m_selectedMeshIdx, snapBefore,
                        snapAfter, "Change texture to " + tex});
          sceneViewport.RebuildChunkBatches(
              localGeometryOverlay.m_selectedChunk,
              fileManager.GetWorkspaceDir());
          localGeometryOverlay.RebuildChunkBatches(
              localGeometryOverlay.m_selectedChunk,
              fileManager.GetWorkspaceDir());
        }
      }
    }
  }

  ImGui::Separator();

  if (testTexture.GetTexture().id != 0) {
    ImGui::Text("Size: %d x %d", testTexture.GetWidth(),
                testTexture.GetHeight());
    // Palette selection & CLUT row controls (Option 2: Steppers + Combo with spanning color bars)
    int numPalettes = (int)testTexture.GetPalettes().size();
    if (numPalettes >= 1) {
      auto applyNewPalette = [&](int newPal) {
        if (numPalettes <= 0) return;
        newPal = (newPal % numPalettes + numPalettes) % numPalettes;
        if (newPal == currentPalette) return;
        currentPalette = newPal;
        testTexture.ApplyPalette(currentPalette);
        if (activeFace) {
          RenderMesh snapBefore = *activeMesh;
          activeFace->paletteRow = currentPalette;
          RenderMesh snapAfter = *activeMesh;
          history.Push(
              {localGeometryOverlay.m_selectedChunk,
               localGeometryOverlay.m_selectedObjectIdx,
               localGeometryOverlay.m_selectedMeshIdx, snapBefore, snapAfter,
               "Change palette to row " + std::to_string(currentPalette)});
          sceneViewport.RebuildChunkBatches(
              localGeometryOverlay.m_selectedChunk,
              fileManager.GetWorkspaceDir());
          localGeometryOverlay.RebuildChunkBatches(
              localGeometryOverlay.m_selectedChunk,
              fileManager.GetWorkspaceDir());
        }
      };

      ImGui::Text("Palette:");
      ImGui::SameLine();

      float btnW = 22.0f;
      float spacing = ImGui::GetStyle().ItemSpacing.x;
      float comboW = ImGui::GetContentRegionAvail().x - (btnW * 2.0f + spacing * 2.0f);
      if (comboW < 80.0f) comboW = 80.0f;

      // Stepper: Previous [◀] (loops around if > 1 palette, disabled if only 1 palette)
      bool canStep = (numPalettes > 1);
      if (!canStep) ImGui::BeginDisabled();
      if (ImGui::ArrowButton("##PrevPal", ImGuiDir_Left)) {
        applyNewPalette(currentPalette - 1);
      }
      if (!canStep) ImGui::EndDisabled();

      ImGui::SameLine();

      // Dropdown Combo
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

          // Draw enumerated label and CLUT color row across the dropdown option
          ImDrawList* drawList = ImGui::GetWindowDrawList();
          std::string numLabel = std::to_string(i);
          float labelW = 24.0f;
          drawList->AddText(ImVec2(itemPos.x + 4.0f, itemPos.y + 1.0f),
                            isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255),
                            numLabel.c_str());

          const auto& rowPal = testTexture.GetPalettes()[i];
          if (!rowPal.colors.empty()) {
            float barX = itemPos.x + labelW;
            float barW = itemW - labelW - 4.0f;
            float barH = itemH - 4.0f;
            float barY = itemPos.y + 2.0f;
            int numCols = (int)rowPal.colors.size();
            float sW = barW / (float)numCols;

            drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(20, 20, 20, 255));
            for (int c = 0; c < numCols; ++c) {
              const auto& col = rowPal.colors[c];
              ImVec2 cp0(barX + c * sW, barY);
              ImVec2 cp1(barX + (c + 1) * sW, barY + barH);
              if (col.a == 0 && col.r == 0 && col.g == 0 && col.b == 0) {
                drawList->AddRectFilled(cp0, cp1, IM_COL32(25, 25, 30, 255));
                drawList->AddLine(cp0, cp1, IM_COL32(70, 70, 80, 255));
              } else {
                drawList->AddRectFilled(cp0, cp1, IM_COL32(col.r, col.g, col.b, 255));
              }
            }
            drawList->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(60, 60, 60, 255));
          }

          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
      if (!canStep) ImGui::EndDisabled();

      ImGui::SameLine();

      // Stepper: Next [▶] (loops around if > 1 palette, disabled if only 1 palette)
      if (!canStep) ImGui::BeginDisabled();
      if (ImGui::ArrowButton("##NextPal", ImGuiDir_Right)) {
        applyNewPalette(currentPalette + 1);
      }
      if (!canStep) ImGui::EndDisabled();
    }

    // 1-row colour preview for the selected CLUT / palette row
    if (!testTexture.GetPalettes().empty()) {
      int palIdx = std::clamp(currentPalette, 0, (int)testTexture.GetPalettes().size() - 1);
      const auto& pal = testTexture.GetPalettes()[palIdx];
      if (!pal.colors.empty()) {
        float availWidth = ImGui::GetContentRegionAvail().x;
        float barHeight = 16.0f;
        ImVec2 barPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImGui::InvisibleButton("##PaletteColorPreview", ImVec2(availWidth, barHeight));
        bool isHovered = ImGui::IsItemHovered();
        ImVec2 mousePos = ImGui::GetMousePos();

        int numColors = (int)pal.colors.size();
        float swatchW = availWidth / (float)numColors;

        int hoveredColorIdx = -1;
        if (isHovered && mousePos.x >= barPos.x && mousePos.x < barPos.x + availWidth) {
          hoveredColorIdx = std::clamp((int)((mousePos.x - barPos.x) / swatchW), 0, numColors - 1);
        }

        drawList->AddRectFilled(barPos, ImVec2(barPos.x + availWidth, barPos.y + barHeight), IM_COL32(20, 20, 20, 255));

        for (int i = 0; i < numColors; ++i) {
          const auto& c = pal.colors[i];
          ImVec2 p0(barPos.x + i * swatchW, barPos.y);
          ImVec2 p1(barPos.x + (i + 1) * swatchW, barPos.y + barHeight);

          if (c.a == 0 && c.r == 0 && c.g == 0 && c.b == 0) {
            drawList->AddRectFilled(p0, p1, IM_COL32(25, 25, 30, 255));
            drawList->AddLine(p0, p1, IM_COL32(70, 70, 80, 255));
          } else {
            drawList->AddRectFilled(p0, p1, IM_COL32(c.r, c.g, c.b, 255));
          }
        }

        drawList->AddRect(barPos, ImVec2(barPos.x + availWidth, barPos.y + barHeight), IM_COL32(80, 80, 80, 255));

        if (hoveredColorIdx >= 0 && hoveredColorIdx < numColors) {
          const auto& hc = pal.colors[hoveredColorIdx];
          ImVec2 hp0(barPos.x + hoveredColorIdx * swatchW, barPos.y);
          ImVec2 hp1(barPos.x + (hoveredColorIdx + 1) * swatchW, barPos.y + barHeight);
          drawList->AddRect(hp0, hp1, IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);

          ImGui::BeginTooltip();
          ImGui::Text("Color #%d / %d", hoveredColorIdx, numColors);
          ImGui::Text("RGB: (%d, %d, %d)", hc.r, hc.g, hc.b);
          ImGui::Text("Hex: #%02X%02X%02X", hc.r, hc.g, hc.b);
          if (hc.a == 0) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Transparent STP)");
          ImGui::EndTooltip();
        }
      }
    }

    ImGui::Checkbox("Snap UVs to 32x32 Grid", &snapToGrid);

    ImVec2 minUV, maxUV;
    RenderMesh snapBefore;
    if (m_canvas.Draw(testTexture, activeFace, activeMesh, snapToGrid, Config::Get().TextureScale, snapBefore, minUV, maxUV)) {
        m_currentTile.minU = minUV.x;
        m_currentTile.minV = minUV.y;
        m_currentTile.maxU = maxUV.x;
        m_currentTile.maxV = maxUV.y;
        m_currentTile.rotationSteps = 0;
        m_currentTile.texName = activeFace->texName;
        m_currentTile.palette = currentPalette;

        PushRecentTile(m_currentTile);
        SaveRecentTiles(fileManager.GetWorkspaceDir());

        RenderMesh snapAfter = *activeMesh;
        history.Push({localGeometryOverlay.m_selectedChunk,
                      localGeometryOverlay.m_selectedObjectIdx,
                      localGeometryOverlay.m_selectedMeshIdx, snapBefore,
                      snapAfter, "Edit UVs"});
        sceneViewport.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            fileManager.GetWorkspaceDir());
        localGeometryOverlay.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            fileManager.GetWorkspaceDir());
    }

    if (activeFace) {
      ImGui::Separator();

      ImGui::Text("Active Tile Settings:");
      if (ImGui::Button("Rotate CCW (90)")) {
        RenderMesh snapBefore = *activeMesh;
        bool isQuad = (activeFace->v[3] != 0xFF);
        int n = isQuad ? 4 : 3;
        // Shift UVs CCW: uv[i] gets uv[(i+1)%n]
        float firstU = activeFace->uv[0][0];
        float firstV = activeFace->uv[0][1];
        for (int i = 0; i < n - 1; ++i) {
          activeFace->uv[i][0] = activeFace->uv[i + 1][0];
          activeFace->uv[i][1] = activeFace->uv[i + 1][1];
        }
        activeFace->uv[n - 1][0] = firstU;
        activeFace->uv[n - 1][1] = firstV;

        m_currentTile.rotationSteps =
            (m_currentTile.rotationSteps + 3) % 4; // -1 = +3
        history.Push({localGeometryOverlay.m_selectedChunk,
                      localGeometryOverlay.m_selectedObjectIdx,
                      localGeometryOverlay.m_selectedMeshIdx, snapBefore,
                      *activeMesh, "Rotate UV CCW"});
        sceneViewport.RebuildChunkBatches(localGeometryOverlay.m_selectedChunk,
                                          fileManager.GetWorkspaceDir());
        localGeometryOverlay.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            fileManager.GetWorkspaceDir());
      }
      ImGui::SameLine();
      if (ImGui::Button("Rotate CW (90)")) {
        RenderMesh snapBefore = *activeMesh;
        bool isQuad = (activeFace->v[3] != 0xFF);
        int n = isQuad ? 4 : 3;
        // Shift UVs CW: uv[i] gets uv[(i-1+n)%n]
        float lastU = activeFace->uv[n - 1][0];
        float lastV = activeFace->uv[n - 1][1];
        for (int i = n - 1; i > 0; --i) {
          activeFace->uv[i][0] = activeFace->uv[i - 1][0];
          activeFace->uv[i][1] = activeFace->uv[i - 1][1];
        }
        activeFace->uv[0][0] = lastU;
        activeFace->uv[0][1] = lastV;

        m_currentTile.rotationSteps = (m_currentTile.rotationSteps + 1) % 4;
        history.Push({localGeometryOverlay.m_selectedChunk,
                      localGeometryOverlay.m_selectedObjectIdx,
                      localGeometryOverlay.m_selectedMeshIdx, snapBefore,
                      *activeMesh, "Rotate UV CW"});
        sceneViewport.RebuildChunkBatches(localGeometryOverlay.m_selectedChunk,
                                          fileManager.GetWorkspaceDir());
        localGeometryOverlay.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            fileManager.GetWorkspaceDir());
      }

      ImGui::Separator();
      if (ImGui::Button("Reset Face to Original State")) {
        if (hasBackup) {
          *activeMesh = originalMeshBackup;
          std::string path = fileManager.GetWorkspaceDir() + "/TIM/" +
                             activeFace->texName + ".TIM";
          if (path != Config::Get().LastTexturePath) {
            if (testTexture.Load(path)) {
              Config::Get().LastTexturePath = path;
              Config::Get().Save();
            }
          }
          currentPalette = activeFace->paletteRow;
          testTexture.ApplyPalette(currentPalette);
          sceneViewport.RebuildChunkBatches(
              localGeometryOverlay.m_selectedChunk,
              fileManager.GetWorkspaceDir());
          localGeometryOverlay.RebuildChunkBatches(
              localGeometryOverlay.m_selectedChunk,
              fileManager.GetWorkspaceDir());
        }
      }

      // Undo / Redo / Save button row
      ImGui::Separator();
      {
        auto syncUiToActiveFace = [&]() {
          if (activeFace && !activeFace->texName.empty()) {
            std::string expectedPath = fileManager.GetWorkspaceDir() +
                                       "/TIM/" + activeFace->texName +
                                       ".TIM";
            if (Config::Get().LastTexturePath != expectedPath ||
                testTexture.GetTexture().id == 0) {
              if (testTexture.Load(expectedPath)) {
                Config::Get().LastTexturePath = expectedPath;
                Config::Get().Save();
                currentPalette = activeFace->paletteRow;
                testTexture.ApplyPalette(currentPalette);
              }
            } else if (currentPalette != activeFace->paletteRow) {
              currentPalette = activeFace->paletteRow;
              testTexture.ApplyPalette(currentPalette);
            }
          }
        };

        bool canUndo = history.CanUndo();
        bool canRedo = history.CanRedo();
        if (!canUndo)
          ImGui::BeginDisabled();
        if (ImGui::Button("Undo (Z)")) {
          if (history.Undo(sceneViewport, localGeometryOverlay,
                           fileManager.GetWorkspaceDir())) {
            syncUiToActiveFace();
          }
        }
        if (canUndo && ImGui::IsItemHovered() &&
            !history.PeekUndoDesc().empty())
          ImGui::SetTooltip("Undo: %s", history.PeekUndoDesc().c_str());
        if (!canUndo)
          ImGui::EndDisabled();

        ImGui::SameLine();
        if (!canRedo)
          ImGui::BeginDisabled();
        if (ImGui::Button("Redo (Y)")) {
          if (history.Redo(sceneViewport, localGeometryOverlay,
                           fileManager.GetWorkspaceDir())) {
            syncUiToActiveFace();
          }
        }
        if (canRedo && ImGui::IsItemHovered() &&
            !history.PeekRedoDesc().empty())
          ImGui::SetTooltip("Redo: %s", history.PeekRedoDesc().c_str());
        if (!canRedo)
          ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Save Chunk (S)") && activeObjName) {
          std::string workDir = fileManager.GetWorkspaceDir();
          std::string chunk = localGeometryOverlay.m_selectedChunk;
          std::string ipdPath = workDir + "/IPD/" + chunk + ".IPD";
          std::string prefix = DeriveChunkPrefix(chunk);
          std::string glbPath = workDir + "/PLM/" + prefix + "_GLB.PLM";
          ParsedChunk *cd = nullptr;
          for (auto &lc : localGeometryOverlay.GetChunks())
            if (lc.data->chunkName == chunk) {
              cd = lc.data.get();
              break;
            }
          if (cd) {
            int n = 0;
            bool written = false;
            bool ok =
                IPDWrite::WriteChunk(ipdPath, glbPath, *cd, &n, &written);
            if (ok) {
              if (written) {
                fileManager.Log("[SAVE] Wrote " + chunk + ".IPD (" +
                                    std::to_string(n) + " face(s) patched)");
              }
            } else {
              fileManager.Log("[SAVE] FAILED to write " + chunk + ".IPD",
                                  true);
            }
          }
        }
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Save all face edits directly to "
                            "workspace/IPD/*.IPD (Ctrl-S)");

        ImGui::SameLine();
        if (ImGui::Button("Validate")) {
          const ParsedChunk *cd = nullptr;
          for (const auto &lc : localGeometryOverlay.GetChunks())
            if (lc.data->chunkName == localGeometryOverlay.m_selectedChunk) {
              cd = lc.data.get();
              break;
            }
          if (cd) {
            auto warnings = IPDWrite::Validate(*cd);
            if (warnings.empty()) {
              fileManager.Log("[VALIDATE] No issues found", false);
            } else {
              for (const auto &w : warnings)
                fileManager.Log("[VALIDATE] " + w, true);
            }
          }
        }
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Check structural integrity (future: will surface "
                            "size/UV issues)");
      }
      ImGui::Separator();
    }
  } else {
    ImGui::Text("No TIM texture file selected.");
  }

  ImGui::Separator();
  ImGui::Checkbox("Enable Tile Paint Mode", &m_tilePaintModeActive);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("When enabled, click and drag in the 3D viewport to "
                      "paint the active tile.");

  ImGui::Separator();
  int activeTexW = testTexture.GetWidth() > 0 ? testTexture.GetWidth() : 256;
  int activeTexH = testTexture.GetHeight() > 0 ? testTexture.GetHeight() : 256;
  int tw = (int)((m_currentTile.maxU - m_currentTile.minU) * (float)activeTexW);
  int th = (int)((m_currentTile.maxV - m_currentTile.minV) * (float)activeTexH);
  ImGui::Text("Active Tile: %s (%dx%d)",
              m_currentTile.texName.empty() ? "None"
                                            : m_currentTile.texName.c_str(),
              tw, th);
  ImGui::Text("Rotation: %d degrees CW", m_currentTile.rotationSteps * 90);

  if (ImGui::Button("Save Active Tile to Cache") &&
      !m_currentTile.texName.empty()) {
    PushRecentTile(m_currentTile);
    SaveRecentTiles(fileManager.GetWorkspaceDir());
  }

  ImGui::Text("Recent Tiles Cache:");
  ImGui::SameLine();
  if (ImGui::Button("Clear Cache")) {
    m_recentTiles.erase(
        std::remove_if(m_recentTiles.begin(), m_recentTiles.end(),
                       [](const SelectedTile &t) { return !t.isPinned; }),
        m_recentTiles.end());
    SaveRecentTiles(fileManager.GetWorkspaceDir());
  }

  float availW = ImGui::GetContentRegionAvail().x;
  float tileScale = Config::Get().TextureScale > 0.05f ? Config::Get().TextureScale : (availW / 256.0f);
  if (tileScale <= 0.05f) tileScale = 1.0f;

  float defaultTileSize = std::round(32.0f * tileScale);
  if (defaultTileSize < 16.0f) defaultTileSize = 32.0f;

  int cols = (int)(availW / defaultTileSize);
  if (cols < 1) cols = 1;

  float scrollChildH = defaultTileSize * 2.0f + 4.0f;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

  ImGui::BeginChild("RecentTilesGrid", ImVec2(0, scrollChildH), true);

  auto drawTileItem = [&](size_t idx) {
    if (idx >= m_recentTiles.size()) return;
    auto &t = m_recentTiles[idx];
    ImGui::PushID((int)idx);

    Texture2D cachedTex = TextureCache::Get().Fetch(
        t.texName, t.palette, fileManager.GetWorkspaceDir());

    int ctw = cachedTex.width > 0 ? cachedTex.width : 256;
    int cth = cachedTex.height > 0 ? cachedTex.height : 256;
    float tw = (t.maxU - t.minU) * (float)ctw;
    float th = (t.maxV - t.minV) * (float)cth;

    float displayW = defaultTileSize;
    float displayH = defaultTileSize;

    ImVec2 uv0(t.minU, t.minV);
    ImVec2 uv1(t.maxU, t.maxV);

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1(p0.x + displayW, p0.y + displayH);

    ImGui::InvisibleButton("##TileBtn", ImVec2(displayW, displayH));
    bool isHovered = ImGui::IsItemHovered();
    bool isClicked = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool isRightClicked = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (cachedTex.id != 0) {
      drawList->AddImage((ImTextureID)(intptr_t)cachedTex.id, p0, p1, uv0, uv1, IM_COL32(255, 255, 255, 255));
    } else {
      drawList->AddRectFilled(p0, p1, IM_COL32(40, 40, 48, 255));
      drawList->AddText(ImVec2(p0.x + 2, p0.y + 2), IM_COL32(180, 180, 180, 255), t.texName.c_str());
    }

    bool isCurrent = (m_currentTile.texName == t.texName &&
                      std::abs(m_currentTile.minU - t.minU) < 0.001f &&
                      std::abs(m_currentTile.minV - t.minV) < 0.001f &&
                      m_currentTile.palette == t.palette);

    if (isCurrent) {
      drawList->AddRect(p0, p1, IM_COL32(0, 220, 255, 255), 0.0f, 0, 2.0f);
    } else if (isHovered) {
      drawList->AddRect(p0, p1, IM_COL32(255, 255, 255, 180), 0.0f, 0, 1.0f);
    }

    if (t.isPinned) {
      float pinX = p1.x - 7.0f;
      float pinY = p0.y + 7.0f;

      drawList->AddCircleFilled(ImVec2(pinX, pinY), 5.5f, IM_COL32(0, 0, 0, 190));
      drawList->AddCircle(ImVec2(pinX, pinY), 5.5f, IM_COL32(255, 200, 0, 220), 0, 1.0f);
      drawList->AddCircleFilled(ImVec2(pinX + 1.0f, pinY - 1.5f), 2.0f, IM_COL32(255, 220, 50, 255));
      drawList->AddLine(ImVec2(pinX + 0.5f, pinY - 0.5f), ImVec2(pinX - 2.0f, pinY + 2.0f), IM_COL32(230, 230, 230, 255), 1.5f);
    }

    if (isClicked) {
      m_currentTile = t;
      std::string expectedPath = fileManager.GetWorkspaceDir() + "/TIM/" + t.texName + ".TIM";
      if (Config::Get().LastTexturePath != expectedPath) {
        if (testTexture.Load(expectedPath)) {
          Config::Get().LastTexturePath = expectedPath;
          Config::Get().Save();
        }
      }
      if (currentPalette != t.palette) {
        currentPalette = t.palette;
        testTexture.ApplyPalette(currentPalette);
      }
    }

    if (isRightClicked) {
      t.isPinned = !t.isPinned;
      std::stable_partition(m_recentTiles.begin(), m_recentTiles.end(),
                            [](const SelectedTile &st) { return st.isPinned; });
      SaveRecentTiles(fileManager.GetWorkspaceDir());
    }

    if (isHovered) {
      ImGui::BeginTooltip();
      ImGui::Text("%s (CLUT %d)", t.texName.c_str(), t.palette);
      ImGui::Text("Size: %dx%d px", (int)std::round(tw), (int)std::round(th));
      if (t.rotationSteps != 0) {
        ImGui::Text("Rotation: %d°", t.rotationSteps * 90);
      }
      if (t.isPinned) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "★ Pinned (Right-click to unpin)");
      } else {
        ImGui::TextDisabled("Right-click to pin");
      }
      ImGui::EndTooltip();
    }

    ImGui::PopID();
  };

  if (m_recentTiles.empty()) {
    ImGui::TextDisabled("  (No recent tiles in cache)");
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

  ImGui::End();
}
