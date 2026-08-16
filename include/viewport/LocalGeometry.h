#pragma once
#include "core/IPDParse.h"
#include "raylib.h"
#include "rlgl.h"
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "viewport/ViewportBase.h"
#include "viewport/ViewportOverlay.h"

class History;
class TextureMapPanel;

enum class EditMode { GlobalObject, Mesh, Face, Vertex };

struct SelectedVertex {
  std::string chunkName;
  int objectIdx;
  int meshIdx;
  int vertexIdx;

  bool operator<(const SelectedVertex &o) const {
    if (chunkName != o.chunkName)
      return chunkName < o.chunkName;
    if (objectIdx != o.objectIdx)
      return objectIdx < o.objectIdx;
    if (meshIdx != o.meshIdx)
      return meshIdx < o.meshIdx;
    return vertexIdx < o.vertexIdx;
  }
};

struct SelectedFace {
  std::string chunkName;
  int objectIdx;
  int meshIdx;
  int faceIdx;

  bool operator<(const SelectedFace &o) const {
    if (chunkName != o.chunkName)
      return chunkName < o.chunkName;
    if (objectIdx != o.objectIdx)
      return objectIdx < o.objectIdx;
    if (meshIdx != o.meshIdx)
      return meshIdx < o.meshIdx;
    return faceIdx < o.faceIdx;
  }
};

class Viewport;

class LocalGeometryOverlay : public ViewportOverlay {
public:
  LocalGeometryOverlay();
  ~LocalGeometryOverlay();

  // Load a parsed chunk into the scene.
  // workspaceDir is used for TIM and GLB lookups.
  bool LoadChunk(std::shared_ptr<ParsedChunk> parsedChunk,
                 const std::string &workspaceDir);

  // Remove a loaded chunk by name (e.g. "THR0000")
  void UnloadChunk(const std::string &chunkName);

  // Set shared chunk geometry pointer from Viewport (zero extra VRAM / VAO
  // overhead)
  void SetSharedChunks(const std::vector<LoadedChunk> *sharedChunks) {
    m_sharedChunks = sharedChunks;
  }

  // Access loaded chunks (for OutlinerPanel / internal rendering)
  const std::vector<LoadedChunk> &GetChunks() const {
    return m_sharedChunks ? *m_sharedChunks : m_chunks;
  }

  std::vector<std::string> ConsumeModifiedChunks();
  std::string m_selectedChunk;
  int m_selectedObjectIdx = -1;

  EditMode m_editMode = EditMode::Face;
  int m_selectedMeshIdx = -1;
  int m_selectedFaceIdx = -1;   // Single select / active focus
  int m_selectedVertexIdx = -1; // Single select / active focus
  std::set<SelectedVertex> m_selectedVertices;
  std::set<SelectedFace> m_selectedFaces;

  History *m_history = nullptr;
  TextureMapPanel *m_texManager = nullptr;

  std::string m_lastWorkspaceDir = "data/workspace";

  // Rebuild GPU batches for a specific chunk after modifying its geometry
  void RebuildChunkBatches(const std::string &chunkName,
                           const std::string &workspaceDir);

  int m_moveStepPower = 4;
  bool m_autoValidate = true;

  void DrawOverlay(Viewport &vp) override;
  void UnloadAll() override;
  void HandlePicking(Viewport &vp, Ray ray) override;
  void HandleBoxPicking(Viewport &vp, Rectangle box) override;
  void DrawContextMenu() override;

  std::vector<ChunkLocation> GetChunkLocations() const;
  std::function<Color(const std::string&)> m_legendColorCallback;

private:
  void HandleTilePainting(Viewport &vp, Ray ray);
  void TranslateSelection(Vector3 delta);
  const std::vector<LoadedChunk> *m_sharedChunks = nullptr;
  std::vector<LoadedChunk> m_chunks;

  std::vector<std::string> m_modifiedChunks;
  void BuildGpuBatches(LoadedChunk &chunk, const std::string &workspaceDir);

  // Free GPU resources for one chunk
  void FreeGpuBatches(LoadedChunk &chunk);

  std::optional<RenderObject> m_clipboardObject;
};
