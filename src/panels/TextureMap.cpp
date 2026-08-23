#include "panels/TextureMap.h"
#include "viewport/Viewport.h"
#include "core/Config.h"
#include "core/FileDialog.h"
#include "core/ChunkManager.h"
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
                                ChunkManager &pipelineManager,
                                DependencyManager &dependencyManager,
                                Viewport &sceneViewport,
                                LocalGeometryOverlay &localGeometryOverlay,
                                History &history) {
  static bool loadedRecentTiles = false;
  if (!loadedRecentTiles) {
    LoadRecentTiles(pipelineManager.GetWorkspaceDir());
    
    auto it = m_recentTiles.begin();
    while (it != m_recentTiles.end()) {
        std::string path = pipelineManager.GetWorkspaceDir() + "/textures/" + it->texName + ".TIM";
        if (!std::filesystem::exists(path)) {
            it = m_recentTiles.erase(it);
        } else {
            ++it;
        }
    }
    SaveRecentTiles(pipelineManager.GetWorkspaceDir());

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
      std::string path = pipelineManager.GetWorkspaceDir() + "/textures/" +
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
    }
  }

  float labelWidth = 110.0f;
  float browseWidth = 80.0f;

  std::vector<std::string> selectedChunks = pipelineManager.GetSelectedChunks();
  if (selectedChunks.empty()) {
    selectedChunks = pipelineManager.GetViewportChunks();
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

  std::string currentWorkspaceDir = pipelineManager.GetWorkspaceDir();
  std::string currentPrefix = pipelineManager.GetSelectedPrefix();
  if (currentWorkspaceDir != lastWorkspaceDirForTex || currentPrefix != lastSelectedPrefixForTex || ImGui::GetTime() - lastTexRefreshTime > 1.0) {
    lastWorkspaceDirForTex = currentWorkspaceDir;
    lastSelectedPrefixForTex = currentPrefix;
    lastTexRefreshTime = ImGui::GetTime();
    cachedTextures.clear();
    
    std::string texDir = currentWorkspaceDir + "/textures/";
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
                                        pipelineManager.GetWorkspaceDir());
      localGeometryOverlay.RebuildChunkBatches(
          cName, pipelineManager.GetWorkspaceDir());
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
              pipelineManager.GetWorkspaceDir() + "/textures/" + tex + ".TIM";
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
                  pipelineManager.GetWorkspaceDir());
              localGeometryOverlay.RebuildChunkBatches(
                  localGeometryOverlay.m_selectedChunk,
                  pipelineManager.GetWorkspaceDir());
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
                                          pipelineManager.GetWorkspaceDir());
        localGeometryOverlay.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            pipelineManager.GetWorkspaceDir());
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
              pipelineManager.GetWorkspaceDir());
          localGeometryOverlay.RebuildChunkBatches(
              localGeometryOverlay.m_selectedChunk,
              pipelineManager.GetWorkspaceDir());
        }
      }
    }
  }

  ImGui::Separator();

  if (testTexture.GetTexture().id != 0) {
    ImGui::Text("Size: %d x %d", testTexture.GetWidth(),
                testTexture.GetHeight());
    if (testTexture.GetPalettes().size() > 1) {
      if (ImGui::SliderInt("Palette", &currentPalette, 0,
                           testTexture.GetPalettes().size() - 1)) {
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
              pipelineManager.GetWorkspaceDir());
          localGeometryOverlay.RebuildChunkBatches(
              localGeometryOverlay.m_selectedChunk,
              pipelineManager.GetWorkspaceDir());
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
        SaveRecentTiles(pipelineManager.GetWorkspaceDir());

        RenderMesh snapAfter = *activeMesh;
        history.Push({localGeometryOverlay.m_selectedChunk,
                      localGeometryOverlay.m_selectedObjectIdx,
                      localGeometryOverlay.m_selectedMeshIdx, snapBefore,
                      snapAfter, "Edit UVs"});
        sceneViewport.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            pipelineManager.GetWorkspaceDir());
        localGeometryOverlay.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            pipelineManager.GetWorkspaceDir());
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
                                          pipelineManager.GetWorkspaceDir());
        localGeometryOverlay.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            pipelineManager.GetWorkspaceDir());
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
                                          pipelineManager.GetWorkspaceDir());
        localGeometryOverlay.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            pipelineManager.GetWorkspaceDir());
      }

      ImGui::Separator();
      if (ImGui::Button("Reset Face to Original State")) {
        if (hasBackup) {
          *activeMesh = originalMeshBackup;
          std::string path = pipelineManager.GetWorkspaceDir() + "/textures/" +
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
              pipelineManager.GetWorkspaceDir());
          localGeometryOverlay.RebuildChunkBatches(
              localGeometryOverlay.m_selectedChunk,
              pipelineManager.GetWorkspaceDir());
        }
      }

      // Undo / Redo / Save button row
      ImGui::Separator();
      {
        auto syncUiToActiveFace = [&]() {
          if (activeFace && !activeFace->texName.empty()) {
            std::string expectedPath = pipelineManager.GetWorkspaceDir() +
                                       "/textures/" + activeFace->texName +
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
                           pipelineManager.GetWorkspaceDir())) {
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
                           pipelineManager.GetWorkspaceDir())) {
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
          std::string workDir = pipelineManager.GetWorkspaceDir();
          std::string chunk = localGeometryOverlay.m_selectedChunk;
          std::string ipdPath = workDir + "/chunks/" + chunk + ".IPD";
          std::string prefix = DeriveChunkPrefix(chunk);
          std::string glbPath = workDir + "/geometry/" + prefix + "_GLB.PLM";
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
                pipelineManager.Log("[SAVE] Wrote " + chunk + ".IPD (" +
                                    std::to_string(n) + " face(s) patched)");
              }
            } else {
              pipelineManager.Log("[SAVE] FAILED to write " + chunk + ".IPD",
                                  true);
            }
          }
        }
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Save all face edits directly to "
                            "workspace/chunks/*.IPD (Ctrl-S)");

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
              pipelineManager.Log("[VALIDATE] No issues found", false);
            } else {
              for (const auto &w : warnings)
                pipelineManager.Log("[VALIDATE] " + w, true);
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
  int tw = (int)((m_currentTile.maxU - m_currentTile.minU) * 256.0f);
  int th = (int)((m_currentTile.maxV - m_currentTile.minV) * 256.0f);
  ImGui::Text("Active Tile: %s (%dx%d)",
              m_currentTile.texName.empty() ? "None"
                                            : m_currentTile.texName.c_str(),
              tw, th);
  ImGui::Text("Rotation: %d degrees CW", m_currentTile.rotationSteps * 90);

  if (ImGui::Button("Save Active Tile to Cache") &&
      !m_currentTile.texName.empty()) {
    PushRecentTile(m_currentTile);
    SaveRecentTiles(pipelineManager.GetWorkspaceDir());
  }

  ImGui::Text("Recent Tiles Cache:");
  ImGui::SameLine();
  if (ImGui::Button("Clear Cache")) {
    m_recentTiles.erase(
        std::remove_if(m_recentTiles.begin(), m_recentTiles.end(),
                       [](const SelectedTile &t) { return !t.isPinned; }),
        m_recentTiles.end());
    SaveRecentTiles(pipelineManager.GetWorkspaceDir());
  }

  ImGui::BeginChild("RecentTilesScroll", ImVec2(0, 90), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  for (size_t i = 0; i < m_recentTiles.size(); i++) {
    auto &t = m_recentTiles[i];
    ImGui::PushID((int)i);

    int w = (int)((t.maxU - t.minU) * 256.0f);
    int h = (int)((t.maxV - t.minV) * 256.0f);

    Texture2D cachedTex = TextureCache::Get().Fetch(
        t.texName, t.palette, pipelineManager.GetWorkspaceDir());

    ImVec2 uv0(t.minU, t.minV);
    ImVec2 uv1(t.maxU, t.maxV);

    ImVec4 bgCol =
        t.isPinned ? ImVec4(0.8f, 0.6f, 0.0f, 1.0f) : ImVec4(0, 0, 0, 0);

    if (cachedTex.id != 0) {
      if (ImGui::ImageButton("##RecentTileImage",
                             (ImTextureID)(intptr_t)cachedTex.id,
                             ImVec2(64, 64), uv0, uv1, bgCol)) {
        m_currentTile = t;
        // Also update the main Texture Manager view
        std::string expectedPath = pipelineManager.GetWorkspaceDir() +
                                   "/textures/" + t.texName + ".TIM";
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
    } else {
      char buf[64];
      snprintf(buf, sizeof(buf), "%s\n%dx%d", t.texName.c_str(), w, h);
      if (t.isPinned)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.0f, 1.0f));
      if (ImGui::Button(buf, ImVec2(64, 64))) {
        m_currentTile = t;
      }
      if (t.isPinned)
        ImGui::PopStyleColor();
    }

    if (ImGui::IsItemHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      t.isPinned = !t.isPinned;
      SaveRecentTiles(pipelineManager.GetWorkspaceDir());
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s (Palette %d) - %dx%d", t.texName.c_str(), t.palette,
                        w, h);
    }
    ImGui::SameLine();
    ImGui::PopID();
  }
  ImGui::EndChild();

  ImGui::End();
}
