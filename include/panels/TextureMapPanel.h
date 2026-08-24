#pragma once
#include "core/FileManager.h"
#include "core/History.h"
#include "core/Textures.h"
#include "formats/Structs.h"
#include "imgui.h"
#include "raylib.h"
#include "viewport/LocalGeometry.h"
#include "viewport/Scene.h"
#include "panels/TextureEditPanel.h"
#include <deque>
#include <set>
#include <string>
#include <vector>

class DependencyManager;
class Viewport;

class TextureMapPanel {
public:
  void Draw(Textures &testTexture, int &currentPalette,
            FileManager &fileManager, DependencyManager &dependencyManager, Viewport &sceneViewport,
            LocalGeometryOverlay &localGeometryOverlay, History &history);

  struct SelectedTile {
    float minU, minV, maxU, maxV;
    int rotationSteps; // 0=0deg, 1=90deg CW, 2=180deg, 3=270deg
    std::string texName;
    int palette;
    bool isPinned = false;

    bool operator==(const SelectedTile &other) const {
      return (texName == other.texName && palette == other.palette &&
              minU == other.minU && minV == other.minV && maxU == other.maxU &&
              maxV == other.maxV && rotationSteps == other.rotationSteps);
    }
  };

  bool IsTilePaintModeActive() const { return m_tilePaintModeActive; }
  const SelectedTile &GetCurrentTile() const { return m_currentTile; }

private:
  // Sync face selection state
  std::string lastSelChunk;
  int lastSelObj = -1;
  int lastSelMesh = -1;
  int lastSelFace = -1;

  RenderMesh originalMeshBackup;
  bool hasBackup = false;

  // Texture selection state
  std::string lastWorkspaceDirForTex;
  std::string lastSelectedPrefixForTex;
  std::vector<std::string> cachedTextures;
  double lastTexRefreshTime = 0.0;

  bool snapToGrid = true;

  // Tile Paint & Recent Tiles state
  bool m_tilePaintModeActive = false;

  std::deque<SelectedTile> m_recentTiles;
  size_t m_maxRecentTiles = 16;
  SelectedTile m_currentTile;

  void PushRecentTile(const SelectedTile &tile);

  // JSON cache for recent tiles
  TextureEditPanel m_canvas;
  
  void LoadRecentTiles(const std::string &workspaceDir);
  void SaveRecentTiles(const std::string &workspaceDir);
};
