#pragma once
#include "core/IPDParse.h"
#include "core/OverlayLoader.h"
#include <deque>
#include <string>

// ---------------------------------------------------------------------------
// MeshSnapshot — a before/after record of a mesh edit operation.
// ---------------------------------------------------------------------------
struct MeshSnapshot {
  std::string chunkName;
  int objectIdx;
  int meshIdx;
  RenderMesh before;
  RenderMesh after;
  std::string description;
};

// ---------------------------------------------------------------------------
// OverlaySnapshot — a before/after record of a door link or waypoint edit.
// ---------------------------------------------------------------------------
struct OverlaySnapshot {
  std::string mapKey;
  OverlayMapData before;
  OverlayMapData after;
  std::string description;
};

enum class EditType { Mesh, Overlay };

struct EditRecord {
  EditType type = EditType::Mesh;
  MeshSnapshot meshSnap;
  OverlaySnapshot overlaySnap;
};

// ---------------------------------------------------------------------------
// History — global undo/redo stack across 3D meshes and map overlays.
// ---------------------------------------------------------------------------
class History {
public:
  explicit History(int maxDepth = 50);

  // Push: record a new edit (mesh or overlay).
  void Push(MeshSnapshot snap);
  void Push(OverlaySnapshot snap);

  // Undo / Redo
  bool Undo(class Viewport &vp, class LocalGeometryOverlay &evp,
            class WaypointsOverlay *eventVp, const std::string &workspaceDir);
  bool Redo(class Viewport &vp, class LocalGeometryOverlay &evp,
            class WaypointsOverlay *eventVp, const std::string &workspaceDir);

  // Convenience overloads for 3 arguments
  bool Undo(class Viewport &vp, class LocalGeometryOverlay &evp,
            const std::string &workspaceDir) {
    return Undo(vp, evp, nullptr, workspaceDir);
  }
  bool Redo(class Viewport &vp, class LocalGeometryOverlay &evp,
            const std::string &workspaceDir) {
    return Redo(vp, evp, nullptr, workspaceDir);
  }

  bool CanUndo() const { return !m_undo.empty(); }
  bool CanRedo() const { return !m_redo.empty(); }

  const std::string &PeekUndoDesc() const;
  const std::string &PeekRedoDesc() const;

  void SetMaxDepth(int depth);
  int GetMaxDepth() const { return m_maxDepth; }

  void Clear();

  static bool IsEqual(const RenderMesh &a, const RenderMesh &b);
  static bool IsEqual(const OverlayMapData &a, const OverlayMapData &b);
  static bool CanMerge(const EditRecord &a, const EditRecord &b);

private:
  int m_maxDepth;
  std::deque<EditRecord> m_undo;
  std::deque<EditRecord> m_redo;

  static const std::string s_emptyDesc;

  static RenderMesh *FindLiveMesh(const std::vector<struct LoadedChunk> &chunks,
                                  const MeshSnapshot &snap);

  static void ApplyMesh(class Viewport &vp,
                        class LocalGeometryOverlay &evp,
                        const std::string &workspaceDir,
                        const MeshSnapshot &snap, const RenderMesh &state);

  static void ApplyOverlay(class WaypointsOverlay *eventVp,
                           const OverlaySnapshot &snap, bool useBeforeState);

  static std::string GetMeshTargetSignature(const MeshSnapshot &snap);
  static std::string GetOverlayTargetSignature(const OverlaySnapshot &snap);
};

