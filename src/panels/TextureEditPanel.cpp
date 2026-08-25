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
    Textures &activeTexture, int &currentPalette, RenderFace *activeFace,
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
    if (activeTexture.Load(Config::Get().LastTexturePath)) {
      currentPalette = 0;
      activeTexture.ApplyPalette(currentPalette);

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
