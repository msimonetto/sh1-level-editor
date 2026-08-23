#include "core/Shortcuts.h"
#include "core/ChunkManager.h"
#include "core/History.h"
#include "formats/IPDParse.h"
#include "formats/IPDWrite.h"
#include "raylib.h"
#include "viewport/Viewport.h"

void Shortcuts::Handle(History &history,
                             ChunkManager &pipelineManager,
                             Viewport &sceneViewport,
                             LocalGeometryOverlay &localGeometryOverlay,
                             WaypointsOverlay *eventViewport) {
  if (IsKeyDown(KEY_LEFT_CONTROL)) {
    if (IsKeyPressed(KEY_Z))
      history.Undo(sceneViewport, localGeometryOverlay, eventViewport,
                   pipelineManager.GetWorkspaceDir());
    if (IsKeyPressed(KEY_Y))
      history.Redo(sceneViewport, localGeometryOverlay, eventViewport,
                   pipelineManager.GetWorkspaceDir());

    if (IsKeyPressed(KEY_S)) {
      if (IsKeyDown(KEY_LEFT_SHIFT)) {
        SaveAll(pipelineManager, sceneViewport);
      } else {
        SaveSelected(pipelineManager, sceneViewport);
      }
    }
  }
}

void Shortcuts::SaveSelected(ChunkManager &pipelineManager,
                                   Viewport &sceneViewport) {
  auto selectedChunks = pipelineManager.GetSelectedChunks();
  if (selectedChunks.empty()) {
    for (const auto &lc : sceneViewport.GetChunks()) {
      if (lc.data && !lc.data->chunkName.empty()) {
        selectedChunks.push_back(lc.data->chunkName);
      }
    }
  }
  if (!selectedChunks.empty()) {
    for (const auto &chunkName : selectedChunks) {
      std::string workspaceDir = pipelineManager.GetWorkspaceDir();
      std::string ipdPath = workspaceDir + "/chunks/" + chunkName + ".IPD";
      std::string glbPath = workspaceDir + "/geometry/" +
                            DeriveChunkPrefix(chunkName) + "_GLB.PLM";

      ParsedChunk *chunkData = nullptr;
      for (auto &lc : sceneViewport.GetChunks()) {
        if (lc.data && lc.data->chunkName == chunkName) {
          chunkData = lc.data.get();
          break;
        }
      }

      if (chunkData) {
        int n = 0;
        bool written = false;
        bool ok =
            IPDWrite::WriteChunk(ipdPath, glbPath, *chunkData, &n, &written);
        if (ok) {
          if (written) {
            pipelineManager.Log("[SAVE] Wrote " + chunkName + ".IPD (" +
                                std::to_string(n) + " face(s) patched)");
          }
        } else {
          pipelineManager.Log("[SAVE] FAILED to write " + chunkName + ".IPD",
                              true);
        }
      } else {
        pipelineManager.Log("[SAVE] Error: Selected chunk data not found. It "
                            "might not be loaded in the viewport.",
                            true);
      }
    }
  } else {
    pipelineManager.Log("[SAVE] No chunk loaded or selected to save.", true);
  }
}

void Shortcuts::SaveAll(ChunkManager &pipelineManager,
                              Viewport &sceneViewport) {
  int count = 0;
  for (auto &lc : sceneViewport.GetChunks()) {
    if (lc.data) {
      std::string chunkName = lc.data->chunkName;
      std::string workspaceDir = pipelineManager.GetWorkspaceDir();
      std::string ipdPath = workspaceDir + "/chunks/" + chunkName + ".IPD";
      std::string glbPath = workspaceDir + "/geometry/" +
                            DeriveChunkPrefix(chunkName) + "_GLB.PLM";

      int n = 0;
      bool written = false;
      bool ok = IPDWrite::WriteChunk(ipdPath, glbPath, *lc.data, &n, &written);
      if (ok && written)
        count++;
    }
  }
  if (count > 0) {
    pipelineManager.Log("[SAVE] Wrote " + std::to_string(count) + " chunks.");
  }
}
