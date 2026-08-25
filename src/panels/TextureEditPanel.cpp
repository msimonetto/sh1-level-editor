#include "panels/TextureEditPanel.h"
#include "core/Config.h"
#include "core/FileDialog.h"
#include "core/FileManager.h"
#include "core/History.h"
#include "viewport/LocalGeometryOverlay.h"
#include "viewport/Viewport.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

void TextureEditPanel::DrawFromFileControls(
    Textures &testTexture, int &currentPalette, RenderFace *activeFace,
    RenderMesh *activeMesh, FileManager &fileManager, Viewport &sceneViewport,
    LocalGeometryOverlay &localGeometryOverlay, History &history) {
  float labelWidth = 110.0f;
  float browseWidth = 80.0f;

  ImGui::AlignTextToFramePadding();
  ImGui::Text("From file:");
  ImGui::SameLine(labelWidth);

  char timPathBuf[256];
  strncpy(timPathBuf, Config::Get().LastTexturePath.c_str(), sizeof(timPathBuf));
  timPathBuf[sizeof(timPathBuf) - 1] = '\0';

  auto applyLoadedTexture = [&](const std::string &path) {
    if (path.empty()) return;
    Config::Get().LastTexturePath = path;
    Config::Get().Save();
    if (testTexture.Load(Config::Get().LastTexturePath)) {
      currentPalette = 0;

      if (activeFace && activeMesh) {
        RenderMesh snapBefore = *activeMesh;
        std::string tex =
            std::filesystem::path(Config::Get().LastTexturePath).stem().string();
        activeFace->texName = tex;
        activeFace->texNum = 0x7F;

        const ParsedChunk *cd = nullptr;
        for (const auto &lc : localGeometryOverlay.GetChunks()) {
          if (lc.data->chunkName == localGeometryOverlay.m_selectedChunk) {
            cd = lc.data.get();
            break;
          }
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
  };

  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth -
                          ImGui::GetStyle().ItemSpacing.x);
  if (ImGui::InputText("##TIM_Path", timPathBuf, sizeof(timPathBuf),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    applyLoadedTexture(timPathBuf);
  }
  ImGui::SameLine();
  if (ImGui::Button("Browse...", ImVec2(browseWidth, 0))) {
    std::string path =
        FileDialog::OpenFile("TIM Files\0*.TIM;*.tim\0All Files\0*.*\0");
    if (!path.empty()) {
      applyLoadedTexture(path);
    }
  }
}

bool TextureEditPanel::Draw(Textures& testTexture, RenderFace* activeFace, RenderMesh* activeMesh, bool snapToGrid, RenderMesh& outSnapBefore, ImVec2& outMinUV, ImVec2& outMaxUV) {
    bool uvsModified = false;

    float availWidth = ImGui::GetContentRegionAvail().x;
    float textureScale = (availWidth > 0.0f) ? (availWidth / 256.0f) : 1.0f;

    ImVec2 imgPos = ImGui::GetCursorScreenPos();
    float imgW = (float)testTexture.GetWidth() * textureScale;
    float imgH = (float)testTexture.GetHeight() * textureScale;

    // Render the Raylib texture in ImGui
    ImGui::Image((ImTextureID)(intptr_t)testTexture.GetTexture().id, ImVec2(imgW, imgH));

    if (ImGui::BeginPopupContextItem("TextureEditContextMenu")) {
        if (ImGui::MenuItem("Reset UV to Original")) {}
        if (ImGui::MenuItem("Save to Recent Tiles")) {}
        if (ImGui::MenuItem("Open in Texture Edit")) {}
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

        ImU32 col = IM_COL32(255, 255, 0, 255);
        float thickness = 2.0f;
        for (int i = 0; i < numVerts; ++i) {
            drawList->AddLine(pts[i], pts[(i + 1) % numVerts], col, thickness);
        }
        for (int i = 0; i < numVerts; ++i) {
            drawList->AddCircleFilled(pts[i], 3.0f, IM_COL32(255, 0, 0, 255));
        }

        // Dragging to update UVs
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
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
                    // Convert screen to texture pixels
                    float tx1 = (p1.x - imgPos.x) / textureScale;
                    float ty1 = (p1.y - imgPos.y) / textureScale;
                    float tx2 = (p2.x - imgPos.x) / textureScale;
                    float ty2 = (p2.y - imgPos.y) / textureScale;

                    // Snap to 32x32 grid
                    tx1 = std::floor(tx1 / 32.0f) * 32.0f;
                    ty1 = std::floor(ty1 / 32.0f) * 32.0f;
                    tx2 = std::floor(tx2 / 32.0f) * 32.0f;
                    ty2 = std::floor(ty2 / 32.0f) * 32.0f;

                    float min_tx = std::min(tx1, tx2);
                    float max_tx = std::max(tx1, tx2) + 32.0f;
                    float min_ty = std::min(ty1, ty2);
                    float max_ty = std::max(ty1, ty2) + 32.0f;

                    // Convert back to screen coords for drawing
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
    }

    return uvsModified;
}
