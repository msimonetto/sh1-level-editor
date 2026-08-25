#include "panels/TextureMapPanel.h"
#include "panels/TextureEditPanel.h"
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
  std::vector<const ParsedChunk *> chunkPtrs;
  for (const auto &lc : localGeometryOverlay.GetChunks()) {
    if (lc.data) {
      chunkPtrs.push_back(lc.data.get());
    }
  }
  m_textureSelector.RefreshAvailable(
      fileManager, dependencyManager, localGeometryOverlay.m_selectedChunk,
      localGeometryOverlay.m_selectedObjectIdx,
      localGeometryOverlay.m_selectedMeshIdx, chunkPtrs);
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
  m_textureSelector.DrawCombo(
      fileManager, Config::Get().LastTexturePath,
      [&](const std::string &path, const std::string &tex) {
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
      });
}

void TextureMapPanel::DrawPaletteControls(
    Textures &activeTexture, int &currentPalette, FileManager &fileManager,
    Viewport &sceneViewport, LocalGeometryOverlay &localGeometryOverlay,
    History &history, RenderFace *activeFace, RenderMesh *activeMesh,
    std::string *activeObjName) {
  PaletteInspectorWidget::Draw(
      activeTexture, currentPalette,
      [&](int newPal) {
        ApplyFaceMutation(
            localGeometryOverlay, sceneViewport, fileManager, history,
            activeFace, activeMesh, activeObjName,
            [&](RenderFace &face, const ParsedChunk *, RenderObject &) {
              face.paletteRow = newPal;
            },
            "Change palette to row " + std::to_string(newPal));
      });
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
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_UP_RIGHT_FROM_SQUARE " Open in Texture Edit")) {
    if (m_textureEditPanel && !Config::Get().LastTexturePath.empty()) {
      m_textureEditPanel->Open(Config::Get().LastTexturePath, currentPalette);
    }
  }

  float availWidth = ImGui::GetContentRegionAvail().x;
  float textureScale = (availWidth > 0.0f) ? (availWidth / 256.0f) : 1.0f;

  ImVec2 imgPos = ImGui::GetCursorScreenPos();
  float imgW = (float)activeTexture.GetWidth() * textureScale;
  float imgH = (float)activeTexture.GetHeight() * textureScale;

  // Render the Raylib texture in ImGui
  ImGui::Image((ImTextureID)(intptr_t)activeTexture.GetTexture().id, ImVec2(imgW, imgH));
  bool isImageHovered = ImGui::IsItemHovered();

  if (ImGui::BeginPopupContextItem("TextureEditContextMenu")) {
    if (ImGui::MenuItem("Reset UV to Original")) {}
    if (ImGui::MenuItem("Save to Recent Tiles")) {}
    if (ImGui::MenuItem("Open in Texture Edit")) {
      if (m_textureEditPanel && !Config::Get().LastTexturePath.empty()) {
        m_textureEditPanel->Open(Config::Get().LastTexturePath, currentPalette);
      }
    }
    ImGui::EndPopup();
  }

  // Overlay UVs if a face is selected
  if (activeFace && !activeFace->texName.empty()) {
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    bool isQuad = (activeFace->v[3] != 0xFF);
    int numVerts = isQuad ? 4 : 3;

    ImVec2 pts[4];
    for (int i = 0; i < numVerts; ++i) {
      pts[i].x = imgPos.x + activeFace->uv[i][0] * imgW;
      pts[i].y = imgPos.y + activeFace->uv[i][1] * imgH;
    }

    bool uvsModified = false;
    ImVec2 outMinUV, outMaxUV;
    RenderMesh outSnapBefore;

    // Single UV coordinate dragging with Middle Mouse Button (MMB)
    if (isImageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
      ImVec2 mousePos = ImGui::GetMousePos();
      int closestIdx = 0;
      float minDistanceSq = 1e9f;
      for (int i = 0; i < numVerts; ++i) {
        float dx = mousePos.x - pts[i].x;
        float dy = mousePos.y - pts[i].y;
        float dSq = dx * dx + dy * dy;
        if (dSq < minDistanceSq) {
          minDistanceSq = dSq;
          closestIdx = i;
        }
      }
      m_isDraggingSingleUV = true;
      m_draggedVertexIdx = closestIdx;
      if (activeMesh) {
        m_dragStartMesh = *activeMesh;
      }
    }

    if (m_isDraggingSingleUV && m_draggedVertexIdx >= 0 && m_draggedVertexIdx < numVerts) {
      if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        float tx = (mousePos.x - imgPos.x) / textureScale;
        float ty = (mousePos.y - imgPos.y) / textureScale;

        if (snapToGrid) {
          tx = std::round(tx / 32.0f) * 32.0f;
          ty = std::round(ty / 32.0f) * 32.0f;
        } else {
          tx = std::round(tx);
          ty = std::round(ty);
        }

        float texW = (float)activeTexture.GetWidth();
        float texH = (float)activeTexture.GetHeight();
        tx = std::clamp(tx, 0.0f, texW);
        ty = std::clamp(ty, 0.0f, texH);

        float newU = (texW > 0.0f) ? (tx / texW) : 0.0f;
        float newV = (texH > 0.0f) ? (ty / texH) : 0.0f;

        activeFace->uv[m_draggedVertexIdx][0] = newU;
        activeFace->uv[m_draggedVertexIdx][1] = newV;

        pts[m_draggedVertexIdx].x = imgPos.x + newU * imgW;
        pts[m_draggedVertexIdx].y = imgPos.y + newV * imgH;

        ImGui::SetTooltip("Vertex %d UV: (%d, %d) px\nU: %.3f, V: %.3f",
                          m_draggedVertexIdx, (int)tx, (int)ty, newU, newV);
      }
      if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
        m_isDraggingSingleUV = false;

        float minU = activeFace->uv[0][0];
        float maxU = activeFace->uv[0][0];
        float minV = activeFace->uv[0][1];
        float maxV = activeFace->uv[0][1];
        for (int i = 1; i < numVerts; ++i) {
          minU = std::min(minU, activeFace->uv[i][0]);
          maxU = std::max(maxU, activeFace->uv[i][0]);
          minV = std::min(minV, activeFace->uv[i][1]);
          maxV = std::max(maxV, activeFace->uv[i][1]);
        }

        outMinUV = ImVec2(minU, minV);
        outMaxUV = ImVec2(maxU, maxV);
        outSnapBefore = m_dragStartMesh;
        uvsModified = true;
        m_draggedVertexIdx = -1;
      }
    }

    // Left mouse button box dragging to update all UVs
    if (isImageHovered && ImGui::IsMouseClicked(0) && !m_isDraggingSingleUV) {
      m_isDraggingUV = true;
      m_dragStartUV = ImGui::GetMousePos();
      m_dragEndUV = m_dragStartUV;
      if (activeMesh) {
        m_dragStartMesh = *activeMesh;
      }
    }

    if (m_isDraggingUV) {
      if (ImGui::IsMouseDown(0)) {
        m_dragEndUV = ImGui::GetMousePos();
        ImVec2 p1 = m_dragStartUV;
        ImVec2 p2 = m_dragEndUV;

        if (snapToGrid) {
          float tx1 = (p1.x - imgPos.x) / textureScale;
          float ty1 = (p1.y - imgPos.y) / textureScale;
          float tx2 = (p2.x - imgPos.x) / textureScale;
          float ty2 = (p2.y - imgPos.y) / textureScale;

          tx1 = std::floor(tx1 / 32.0f) * 32.0f;
          ty1 = std::floor(ty1 / 32.0f) * 32.0f;
          tx2 = std::floor(tx2 / 32.0f) * 32.0f;
          ty2 = std::floor(ty2 / 32.0f) * 32.0f;

          float min_tx = std::min(tx1, tx2);
          float max_tx = std::max(tx1, tx2) + 32.0f;
          float min_ty = std::min(ty1, ty2);
          float max_ty = std::max(ty1, ty2) + 32.0f;

          p1.x = imgPos.x + min_tx * textureScale;
          p1.y = imgPos.y + min_ty * textureScale;
          p2.x = imgPos.x + max_tx * textureScale;
          p2.y = imgPos.y + max_ty * textureScale;
        }

        drawList->AddRect(p1, p2, IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);
      }
      if (ImGui::IsMouseReleased(0)) {
        m_isDraggingUV = false;
        m_dragEndUV = ImGui::GetMousePos();

        ImVec2 p1 = m_dragStartUV;
        ImVec2 p2 = m_dragEndUV;

        if (snapToGrid) {
          float tx1 = std::floor(((p1.x - imgPos.x) / textureScale) / 32.0f) * 32.0f;
          float ty1 = std::floor(((p1.y - imgPos.y) / textureScale) / 32.0f) * 32.0f;
          float tx2 = std::floor(((p2.x - imgPos.x) / textureScale) / 32.0f) * 32.0f;
          float ty2 = std::floor(((p2.y - imgPos.y) / textureScale) / 32.0f) * 32.0f;

          float min_tx = std::min(tx1, tx2);
          float max_tx = std::max(tx1, tx2) + 32.0f;
          float min_ty = std::min(ty1, ty2);
          float max_ty = std::max(ty1, ty2) + 32.0f;

          p1.x = imgPos.x + min_tx * textureScale;
          p1.y = imgPos.y + min_ty * textureScale;
          p2.x = imgPos.x + max_tx * textureScale;
          p2.y = imgPos.y + max_ty * textureScale;
        }

        float u1 = std::clamp((p1.x - imgPos.x) / imgW, 0.0f, 1.0f);
        float v1 = std::clamp((p1.y - imgPos.y) / imgH, 0.0f, 1.0f);
        float u2 = std::clamp((p2.x - imgPos.x) / imgW, 0.0f, 1.0f);
        float v2 = std::clamp((p2.y - imgPos.y) / imgH, 0.0f, 1.0f);

        float minU = std::min(u1, u2);
        float maxU = std::max(u1, u2);
        float minV = std::min(v1, v2);
        float maxV = std::max(v1, v2);

        float cx = 0, cy = 0;
        for (int i = 0; i < numVerts; i++) {
          cx += activeFace->uv[i][0];
          cy += activeFace->uv[i][1];
        }
        cx /= numVerts;
        cy /= numVerts;

        for (int i = 0; i < numVerts; ++i) {
          activeFace->uv[i][0] = (activeFace->uv[i][0] < cx) ? minU : maxU;
          activeFace->uv[i][1] = (activeFace->uv[i][1] < cy) ? minV : maxV;
        }

        outMinUV = ImVec2(minU, minV);
        outMaxUV = ImVec2(maxU, maxV);
        outSnapBefore = m_dragStartMesh;
        uvsModified = true;
      }
    }

    // Draw UV polygon lines
    ImU32 col = IM_COL32(255, 255, 0, 255);
    float thickness = 2.0f;
    for (int i = 0; i < numVerts; ++i) {
      drawList->AddLine(pts[i], pts[(i + 1) % numVerts], col, thickness);
    }

    // Draw UV vertices
    for (int i = 0; i < numVerts; ++i) {
      drawList->AddCircleFilled(pts[i], 3.0f, IM_COL32(255, 0, 0, 255));
    }

    // Hover or Active vertex highlight
    if (m_isDraggingSingleUV && m_draggedVertexIdx >= 0 && m_draggedVertexIdx < numVerts) {
      drawList->AddCircle(pts[m_draggedVertexIdx], 6.0f, IM_COL32(0, 255, 255, 255), 0, 2.0f);
      drawList->AddCircleFilled(pts[m_draggedVertexIdx], 4.0f, IM_COL32(0, 255, 255, 200));
    } else if (isImageHovered && !m_isDraggingUV) {
      ImVec2 mousePos = ImGui::GetMousePos();
      int hoverIdx = 0;
      float minDistanceSq = 1e9f;
      for (int i = 0; i < numVerts; ++i) {
        float dx = mousePos.x - pts[i].x;
        float dy = mousePos.y - pts[i].y;
        float dSq = dx * dx + dy * dy;
        if (dSq < minDistanceSq) {
          minDistanceSq = dSq;
          hoverIdx = i;
        }
      }
      drawList->AddCircle(pts[hoverIdx], 5.5f, IM_COL32(0, 255, 255, 200), 0, 1.5f);
    }

    if (uvsModified) {
      m_currentTile.minU = outMinUV.x;
      m_currentTile.minV = outMinUV.y;
      m_currentTile.maxU = outMaxUV.x;
      m_currentTile.maxV = outMaxUV.y;
      m_currentTile.rotationSteps = 0;
      m_currentTile.texName = activeFace->texName;
      m_currentTile.palette = currentPalette;

      PushRecentTile(m_currentTile);
      SaveRecentTiles(fileManager.GetWorkspaceDir());

      if (activeMesh) {
        RenderMesh snapAfter = *activeMesh;
        history.Push({localGeometryOverlay.m_selectedChunk,
                      localGeometryOverlay.m_selectedObjectIdx,
                      localGeometryOverlay.m_selectedMeshIdx, outSnapBefore,
                      snapAfter, "Edit UVs"});
        sceneViewport.RebuildChunkBatches(localGeometryOverlay.m_selectedChunk,
                                          fileManager.GetWorkspaceDir());
        localGeometryOverlay.RebuildChunkBatches(
            localGeometryOverlay.m_selectedChunk,
            fileManager.GetWorkspaceDir());
      }
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
