#pragma once
#include "core/FileManager.h"
#include "core/History.h"
#include "core/Textures.h"
#include "formats/Structs.h"
#include "imgui.h"
#include "raylib.h"
#include "viewport/LocalGeometryOverlay.h"
#include "viewport/SceneOverlay.h"
#include "panels/TextureWidgets.h"
#include <deque>
#include <functional>
#include <set>
#include <string>
#include <vector>

class DependencyManager;
class Viewport;

class TextureMapPanel {
public:
  void Draw(Textures &activeTexture, int &currentPalette,
            FileManager &fileManager, DependencyManager &dependencyManager,
            Viewport &sceneViewport, LocalGeometryOverlay &localGeometryOverlay,
            History &history);

  struct SelectedTile {
    float minU, minV, maxU, maxV;
    int rotationSteps = 0; // 0=0deg, 1=90deg CW, 2=180deg, 3=270deg
    std::string texName;
    int palette;
    bool isPinned = false;

    bool operator==(const SelectedTile &other) const {
      return (texName == other.texName && palette == other.palette &&
              std::abs(minU - other.minU) < 0.001f &&
              std::abs(minV - other.minV) < 0.001f &&
              std::abs(maxU - other.maxU) < 0.001f &&
              std::abs(maxV - other.maxV) < 0.001f);
    }
  };

  bool IsTilePaintModeActive() const { return m_tilePaintModeActive; }
  const SelectedTile &GetCurrentTile() const { return m_currentTile; }
  TextureFilterScope GetFilterScope() const { return m_textureSelector.GetFilterScope(); }
  void SetFilterScope(TextureFilterScope scope) { m_textureSelector.SetFilterScope(scope); }

private:
  TextureSelectorWidget m_textureSelector;

  // Selection state tracking
  std::string lastSelChunk;
  int lastSelObj = -1;
  int lastSelMesh = -1;
  int lastSelFace = -1;

  bool snapToGrid = true;

  // UV dragging state
  bool m_isDraggingUV = false;
  bool m_isDraggingSingleUV = false;
  int m_draggedVertexIdx = -1;
  ImVec2 m_dragStartUV;
  ImVec2 m_dragEndUV;
  RenderMesh m_dragStartMesh;

  // Tile Paint & Recent Tiles state
  bool m_tilePaintModeActive = false;
  std::deque<SelectedTile> m_recentTiles;
  size_t m_maxRecentTiles = 16;
  SelectedTile m_currentTile;

  // Helper methods
  void EnsureRecentTilesLoaded(const std::string &workspaceDir);
  void PushRecentTile(const SelectedTile &tile);
  void LoadRecentTiles(const std::string &workspaceDir);
  void SaveRecentTiles(const std::string &workspaceDir);

  void SyncSelectionState(LocalGeometryOverlay &localGeometryOverlay,
                          FileManager &fileManager, Textures &activeTexture,
                          int &currentPalette, RenderFace *&activeFace,
                          RenderMesh *&activeMesh, std::string *&activeObjName);

  void RefreshAvailableTextures(FileManager &fileManager,
                                DependencyManager &dependencyManager,
                                LocalGeometryOverlay &localGeometryOverlay);

  void ApplyFaceMutation(
      LocalGeometryOverlay &localGeometryOverlay, Viewport &sceneViewport,
      FileManager &fileManager, History &history, RenderFace *activeFace,
      RenderMesh *activeMesh, std::string *activeObjName,
      const std::function<void(RenderFace &, const ParsedChunk *, RenderObject &)>
          &fn,
      const std::string &desc);

  void DrawTextureSelector(Textures &activeTexture, int &currentPalette,
                           FileManager &fileManager, Viewport &sceneViewport,
                           LocalGeometryOverlay &localGeometryOverlay,
                           History &history, RenderFace *activeFace,
                           RenderMesh *activeMesh, std::string *activeObjName);

  void DrawPaletteControls(Textures &activeTexture, int &currentPalette,
                           FileManager &fileManager, Viewport &sceneViewport,
                           LocalGeometryOverlay &localGeometryOverlay,
                           History &history, RenderFace *activeFace,
                           RenderMesh *activeMesh, std::string *activeObjName);

  void DrawUVCanvas(Textures &activeTexture, int &currentPalette,
                    FileManager &fileManager, Viewport &sceneViewport,
                    LocalGeometryOverlay &localGeometryOverlay, History &history,
                    RenderFace *activeFace, RenderMesh *activeMesh,
                    std::string *activeObjName);

  void DrawRecentTilesGrid(Textures &activeTexture, int &currentPalette,
                           FileManager &fileManager);
};