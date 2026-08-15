#pragma once
#include "core/IPDParse.h"
#include "raylib.h"
#include "rlgl.h"
#include "viewport/ViewportBase.h"
#include "viewport/LocalGeometry.h"
#include "panels/ViewportTools.h"
#include <string>
#include <vector>
class CollisionOverlay;
class WaypointsOverlay;
class LocalGeometryOverlay;
#include "viewport/ViewportOverlay.h"
#include <map>

class Viewport : public ViewportBase {
public:
  Viewport();
  ~Viewport() override;

  // Load a parsed chunk into the scene.
  // workspaceDir is used for TIM and GLB lookups.
  bool LoadChunk(std::shared_ptr<ParsedChunk> parsedChunk,
                 const std::string &workspaceDir);

  // Remove a loaded chunk by name (e.g. "THR0000")
  void UnloadChunk(const std::string &chunkName);

  // Access loaded chunks (for OutlinerPanel)
  const std::vector<LoadedChunk> &GetChunks() const { return m_chunks; }
  std::vector<LoadedChunk> &GetChunks() { return m_chunks; }

  std::string m_selectedChunk;
  int m_selectedObjectIdx = -1;

  std::string m_lastWorkspaceDir = "data/workspace";

  // Rebuild GPU batches for a specific chunk after modifying its geometry
  void RebuildChunkBatches(const std::string &chunkName,
                           const std::string &workspaceDir);

  void SetActiveMode(ViewportMode mode) { m_activeMode = mode; }
  void SetOverlay(ViewportMode mode, ViewportOverlay *overlay) {
    m_overlays[mode] = overlay;
  }
  
  template<typename T>
  T* GetOverlay(ViewportMode mode) {
      if (m_overlays.find(mode) != m_overlays.end()) {
          return static_cast<T*>(m_overlays[mode]);
      }
      return nullptr;
  }
  
  void SetCameraTarget(Vector3 target, float distance, float elevation, float azimuth);

  ViewportMode m_activeMode = ViewportMode::Scene;
  std::map<ViewportMode, ViewportOverlay*> m_overlays;
  void DrawContextMenu() override;
  void Draw() override;
protected:
  void DrawScene() override;
  size_t GetChunkCount() const override { return m_chunks.size(); }
  void OnUnloadAll() override;
  std::vector<ChunkLocation> GetChunkLocations() const override;
  void HandlePicking(Ray ray) override;
  void HandleBoxPicking(Rectangle box) override;

private:
  std::vector<LoadedChunk> m_chunks;

  // Build GpuBatches from a LoadedChunk's parsed data
  void BuildGpuBatches(LoadedChunk &chunk, const std::string &workspaceDir);

  // Free GPU resources for one chunk
  void FreeGpuBatches(LoadedChunk &chunk);
};
