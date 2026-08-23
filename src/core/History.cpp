#include "core/History.h"
#include "formats/OverlayLoader.h"
#include "formats/IPDWrite.h"
#include "viewport/LocalGeometry.h"
#include "viewport/Viewport.h"
#include "viewport/Waypoints.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>

const std::string History::s_emptyDesc = "";

// ---------------------------------------------------------------------------
History::History(int maxDepth) : m_maxDepth(maxDepth) {}

// ---------------------------------------------------------------------------
bool History::IsEqual(const RenderMesh &a, const RenderMesh &b) {
  if (a.vx != b.vx || a.vy != b.vy || a.vz != b.vz)
    return false;
  if (a.faces.size() != b.faces.size())
    return false;
  for (size_t i = 0; i < a.faces.size(); ++i) {
    const auto &fa = a.faces[i];
    const auto &fb = b.faces[i];
    if (fa.v[0] != fb.v[0] || fa.v[1] != fb.v[1] || fa.v[2] != fb.v[2] ||
        fa.v[3] != fb.v[3])
      return false;
    if (fa.texNum != fb.texNum || fa.paletteRow != fb.paletteRow ||
        fa.cbaRaw != fb.cbaRaw)
      return false;
    if (fa.texName != fb.texName)
      return false;
    if (fa.unk1 != fb.unk1 || fa.origTexByte != fb.origTexByte)
      return false;
    for (int k = 0; k < 4; ++k) {
      if (fa.uv[k][0] != fb.uv[k][0] || fa.uv[k][1] != fb.uv[k][1])
        return false;
      if (fa.rawU[k] != fb.rawU[k] || fa.rawV[k] != fb.rawV[k])
        return false;
      if (fa.normals[k] != fb.normals[k])
        return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
bool History::IsEqual(const OverlayMapData &a, const OverlayMapData &b) {
  if (a.mapKey != b.mapKey)
    return false;
  if (a.waypoints.size() != b.waypoints.size() ||
      a.links.size() != b.links.size())
    return false;
  for (size_t i = 0; i < a.waypoints.size(); ++i) {
    const auto &wa = a.waypoints[i];
    const auto &wb = b.waypoints[i];
    if (wa.index != wb.index || wa.rawX != wb.rawX || wa.rawZ != wb.rawZ ||
        wa.worldX != wb.worldX || wa.worldZ != wb.worldZ ||
        wa.arrivalAngleDeg != wb.arrivalAngleDeg ||
        wa.triggerParam0 != wb.triggerParam0 ||
        wa.triggerParam1 != wb.triggerParam1 ||
        wa.paperMapIdx != wb.paperMapIdx ||
        wa.paperMapIdxValue != wb.paperMapIdxValue ||
        wa.loadingScreenId != wb.loadingScreenId ||
        wa.loadingScreenIdValue != wb.loadingScreenIdValue ||
        wa.field_4_5 != wb.field_4_5 || wa.unused_4_12 != wb.unused_4_12)
      return false;
  }
  for (size_t i = 0; i < a.links.size(); ++i) {
    const auto &la = a.links[i];
    const auto &lb = b.links[i];
    if (la.index != lb.index || la.waypointIdx != lb.waypointIdx ||
        la.destMapKey != lb.destMapKey || la.destMapIdx != lb.destMapIdx ||
        la.triggerType != lb.triggerType ||
        la.triggerTypeValue != lb.triggerTypeValue ||
        la.activationType != lb.activationType ||
        la.activationTypeValue != lb.activationTypeValue ||
        la.sysState != lb.sysState || la.sysStateValue != lb.sysStateValue ||
        la.eventParam != lb.eventParam ||
        la.requiredEventFlag != lb.requiredEventFlag ||
        la.disabledEventFlag != lb.disabledEventFlag ||
        la.disabledEventFlagValue != lb.disabledEventFlagValue ||
        la.requiredItemId != lb.requiredItemId ||
        la.requiredItemIdValue != lb.requiredItemIdValue ||
        la.flags_8_13 != lb.flags_8_13 || la.sfxPairIdx != lb.sfxPairIdx ||
        la.sfxPairIdxValue != lb.sfxPairIdxValue ||
        la.field_8_24 != lb.field_8_24)
      return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
std::string History::GetMeshTargetSignature(const MeshSnapshot &snap) {
  std::string sig = snap.chunkName + ":" + std::to_string(snap.objectIdx) +
                    ":" + std::to_string(snap.meshIdx) + "|";

  const auto &va = snap.before;
  const auto &vb = snap.after;

  if (va.vx.size() != vb.vx.size()) {
    sig += "VCNT:" + std::to_string(va.vx.size()) + "->" +
           std::to_string(vb.vx.size()) + ";";
  }
  size_t vCount = std::min(va.vx.size(), vb.vx.size());
  sig += "V:[";
  for (size_t i = 0; i < vCount; ++i) {
    if (va.vx[i] != vb.vx[i] || va.vy[i] != vb.vy[i] || va.vz[i] != vb.vz[i]) {
      sig += std::to_string(i) + ",";
    }
  }
  sig += "];";

  if (va.faces.size() != vb.faces.size()) {
    sig += "FCNT:" + std::to_string(va.faces.size()) + "->" +
           std::to_string(vb.faces.size()) + ";";
  }
  size_t fCount = std::min(va.faces.size(), vb.faces.size());
  sig += "F:[";
  for (size_t i = 0; i < fCount; ++i) {
    const auto &fa = va.faces[i];
    const auto &fb = vb.faces[i];
    bool diff = false;
    if (fa.v[0] != fb.v[0] || fa.v[1] != fb.v[1] || fa.v[2] != fb.v[2] ||
        fa.v[3] != fb.v[3] || fa.texNum != fb.texNum ||
        fa.paletteRow != fb.paletteRow || fa.cbaRaw != fb.cbaRaw ||
        fa.texName != fb.texName || fa.unk1 != fb.unk1 ||
        fa.origTexByte != fb.origTexByte) {
      diff = true;
    } else {
      for (int k = 0; k < 4; ++k) {
        if (fa.uv[k][0] != fb.uv[k][0] || fa.uv[k][1] != fb.uv[k][1] ||
            fa.rawU[k] != fb.rawU[k] || fa.rawV[k] != fb.rawV[k] ||
            fa.normals[k] != fb.normals[k]) {
          diff = true;
          break;
        }
      }
    }
    if (diff) {
      sig += std::to_string(i) + ",";
    }
  }
  sig += "];";

  return sig;
}

// ---------------------------------------------------------------------------
std::string
History::GetOverlayTargetSignature(const OverlaySnapshot &snap) {
  std::string sig = snap.mapKey + "|";

  const auto &oa = snap.before;
  const auto &ob = snap.after;

  if (oa.waypoints.size() != ob.waypoints.size()) {
    sig += "WPCNT:" + std::to_string(oa.waypoints.size()) + "->" +
           std::to_string(ob.waypoints.size()) + ";";
  }
  size_t wpCount = std::min(oa.waypoints.size(), ob.waypoints.size());
  for (size_t i = 0; i < wpCount; ++i) {
    const auto &wa = oa.waypoints[i];
    const auto &wb = ob.waypoints[i];
    if (wa.index != wb.index || wa.rawX != wb.rawX || wa.rawZ != wb.rawZ ||
        wa.worldX != wb.worldX || wa.worldZ != wb.worldZ ||
        wa.arrivalAngleDeg != wb.arrivalAngleDeg ||
        wa.triggerParam0 != wb.triggerParam0 ||
        wa.triggerParam1 != wb.triggerParam1 ||
        wa.paperMapIdx != wb.paperMapIdx ||
        wa.paperMapIdxValue != wb.paperMapIdxValue ||
        wa.loadingScreenId != wb.loadingScreenId ||
        wa.loadingScreenIdValue != wb.loadingScreenIdValue ||
        wa.field_4_5 != wb.field_4_5 || wa.unused_4_12 != wb.unused_4_12) {
      sig += "WP:" + std::to_string(i) + ";";
    }
  }

  if (oa.links.size() != ob.links.size()) {
    sig += "LINKCNT:" + std::to_string(oa.links.size()) + "->" +
           std::to_string(ob.links.size()) + ";";
  }
  size_t linkCount = std::min(oa.links.size(), ob.links.size());
  for (size_t i = 0; i < linkCount; ++i) {
    const auto &la = oa.links[i];
    const auto &lb = ob.links[i];
    if (la.index != lb.index || la.waypointIdx != lb.waypointIdx ||
        la.destMapKey != lb.destMapKey || la.destMapIdx != lb.destMapIdx ||
        la.triggerType != lb.triggerType ||
        la.triggerTypeValue != lb.triggerTypeValue ||
        la.activationType != lb.activationType ||
        la.activationTypeValue != lb.activationTypeValue ||
        la.sysState != lb.sysState || la.sysStateValue != lb.sysStateValue ||
        la.eventParam != lb.eventParam ||
        la.requiredEventFlag != lb.requiredEventFlag ||
        la.disabledEventFlag != lb.disabledEventFlag ||
        la.disabledEventFlagValue != lb.disabledEventFlagValue ||
        la.requiredItemId != lb.requiredItemId ||
        la.requiredItemIdValue != lb.requiredItemIdValue ||
        la.flags_8_13 != lb.flags_8_13 || la.sfxPairIdx != lb.sfxPairIdx ||
        la.sfxPairIdxValue != lb.sfxPairIdxValue ||
        la.field_8_24 != lb.field_8_24) {
      sig += "LINK:" + std::to_string(i) + ";";
    }
  }

  return sig;
}

// ---------------------------------------------------------------------------
bool History::CanMerge(const EditRecord &a, const EditRecord &b) {
  if (a.type != b.type)
    return false;
  if (a.type == EditType::Mesh) {
    if (a.meshSnap.chunkName != b.meshSnap.chunkName ||
        a.meshSnap.objectIdx != b.meshSnap.objectIdx ||
        a.meshSnap.meshIdx != b.meshSnap.meshIdx ||
        a.meshSnap.description != b.meshSnap.description) {
      return false;
    }
    std::string sigA = GetMeshTargetSignature(a.meshSnap);
    std::string sigB = GetMeshTargetSignature(b.meshSnap);
    return sigA == sigB && !sigA.empty();
  } else {
    if (a.overlaySnap.mapKey != b.overlaySnap.mapKey ||
        a.overlaySnap.description != b.overlaySnap.description) {
      return false;
    }
    std::string sigA = GetOverlayTargetSignature(a.overlaySnap);
    std::string sigB = GetOverlayTargetSignature(b.overlaySnap);
    return sigA == sigB && !sigA.empty();
  }
}

// ---------------------------------------------------------------------------
void History::Push(MeshSnapshot snap) {
  if (IsEqual(snap.before, snap.after))
    return;
  m_redo.clear();

  EditRecord rec;
  rec.type = EditType::Mesh;
  rec.meshSnap = std::move(snap);

  if (!m_undo.empty() && CanMerge(m_undo.back(), rec)) {
    m_undo.back().meshSnap.after = std::move(rec.meshSnap.after);
    if (IsEqual(m_undo.back().meshSnap.before, m_undo.back().meshSnap.after)) {
      m_undo.pop_back();
    }
    return;
  }

  m_undo.push_back(std::move(rec));
  while ((int)m_undo.size() > m_maxDepth) {
    m_undo.pop_front();
  }
}

// ---------------------------------------------------------------------------
void History::Push(OverlaySnapshot snap) {
  if (IsEqual(snap.before, snap.after))
    return;
  m_redo.clear();

  EditRecord rec;
  rec.type = EditType::Overlay;
  rec.overlaySnap = std::move(snap);

  if (!m_undo.empty() && CanMerge(m_undo.back(), rec)) {
    m_undo.back().overlaySnap.after = std::move(rec.overlaySnap.after);
    if (IsEqual(m_undo.back().overlaySnap.before,
                m_undo.back().overlaySnap.after)) {
      m_undo.pop_back();
    }
    return;
  }

  m_undo.push_back(std::move(rec));
  while ((int)m_undo.size() > m_maxDepth) {
    m_undo.pop_front();
  }
}

// ---------------------------------------------------------------------------
RenderMesh *History::FindLiveMesh(const std::vector<LoadedChunk> &chunks,
                                  const MeshSnapshot &snap) {
  for (auto &lc : chunks) {
    if (lc.data->chunkName != snap.chunkName)
      continue;
    if (snap.objectIdx < 0 || snap.objectIdx >= (int)lc.data->objects.size())
      return nullptr;
    auto &obj = lc.data->objects[snap.objectIdx];
    if (snap.meshIdx < 0 || snap.meshIdx >= (int)obj.meshes.size())
      return nullptr;
    return &obj.meshes[snap.meshIdx];
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
void History::ApplyMesh(Viewport &vp, LocalGeometryOverlay &evp,
                        const std::string &workspaceDir,
                        const MeshSnapshot &snap, const RenderMesh &state) {
  printf("[History] ApplyMesh to chunk: %s, obj: %d, mesh: %d (Target "
         "vertices: %zu)\n",
         snap.chunkName.c_str(), snap.objectIdx, snap.meshIdx, state.vx.size());

  RenderMesh *liveVp = FindLiveMesh(vp.GetChunks(), snap);
  if (liveVp) {
    *liveVp = state;
    vp.RebuildChunkBatches(snap.chunkName, workspaceDir);
    printf("[History] Successfully applied to Viewport.\n");
  } else {
    printf("[History] WARNING: liveVp not found in Viewport.\n");
  }

  RenderMesh *liveEvp = FindLiveMesh(evp.GetChunks(), snap);
  if (liveEvp) {
    *liveEvp = state;
    evp.RebuildChunkBatches(snap.chunkName, workspaceDir);
    printf("[History] Successfully applied to LocalGeometryOverlay.\n");
  } else {
    printf("[History] WARNING: liveEvp not found in LocalGeometryOverlay.\n");
  }
}

// ---------------------------------------------------------------------------
void History::ApplyOverlay(WaypointsOverlay *eventVp,
                           const OverlaySnapshot &snap,
                           bool useBeforeState) {
  const OverlayMapData &state = useBeforeState ? snap.before : snap.after;
  if (eventVp) {
    if (!eventVp->GetOverlay().loaded ||
        eventVp->GetOverlay().mapKey != snap.mapKey) {
      eventVp->LoadOverlay(snap.mapKey);
    }
    eventVp->GetOverlay() = state;
    eventVp->GetOverlay().dirty = true;

    if (eventVp->GetSelectedWaypointIdx() >= (int)state.waypoints.size()) {
      eventVp->SetSelectedWaypoint(-1);
    }
    if (eventVp->GetSelectedLinkIdx() >= (int)state.links.size()) {
      eventVp->SetSelectedLink(-1);
    }
  }
  OverlayLoader::Save(snap.mapKey, state);
  printf(
      "[History] ApplyOverlay for map: %s (Waypoints: %zu, Links: %zu)\n",
      snap.mapKey.c_str(), state.waypoints.size(), state.links.size());
}

// ---------------------------------------------------------------------------
bool History::Undo(Viewport &vp, LocalGeometryOverlay &evp,
                   WaypointsOverlay *eventVp,
                   const std::string &workspaceDir) {
  if (m_undo.empty())
    return false;
  EditRecord record = std::move(m_undo.back());
  m_undo.pop_back();

  if (record.type == EditType::Mesh) {
    ApplyMesh(vp, evp, workspaceDir, record.meshSnap, record.meshSnap.before);
    printf("[History] Undo Mesh: %s\n",
           record.meshSnap.description.c_str());
  } else if (record.type == EditType::Overlay) {
    ApplyOverlay(eventVp, record.overlaySnap, true);
    printf("[History] Undo Overlay: %s\n",
           record.overlaySnap.description.c_str());
  }

  if (!m_redo.empty() && CanMerge(m_redo.back(), record)) {
    if (record.type == EditType::Mesh) {
      m_redo.back().meshSnap.before = std::move(record.meshSnap.before);
      if (IsEqual(m_redo.back().meshSnap.before,
                  m_redo.back().meshSnap.after)) {
        m_redo.pop_back();
      }
    } else {
      m_redo.back().overlaySnap.before = std::move(record.overlaySnap.before);
      if (IsEqual(m_redo.back().overlaySnap.before,
                  m_redo.back().overlaySnap.after)) {
        m_redo.pop_back();
      }
    }
  } else {
    m_redo.push_back(std::move(record));
  }
  return true;
}

// ---------------------------------------------------------------------------
bool History::Redo(Viewport &vp, LocalGeometryOverlay &evp,
                   WaypointsOverlay *eventVp,
                   const std::string &workspaceDir) {
  if (m_redo.empty())
    return false;
  EditRecord record = std::move(m_redo.back());
  m_redo.pop_back();

  if (record.type == EditType::Mesh) {
    ApplyMesh(vp, evp, workspaceDir, record.meshSnap, record.meshSnap.after);
    printf("[History] Redo Mesh: %s\n",
           record.meshSnap.description.c_str());
  } else if (record.type == EditType::Overlay) {
    ApplyOverlay(eventVp, record.overlaySnap, false);
    printf("[History] Redo Overlay: %s\n",
           record.overlaySnap.description.c_str());
  }

  if (!m_undo.empty() && CanMerge(m_undo.back(), record)) {
    if (record.type == EditType::Mesh) {
      m_undo.back().meshSnap.after = std::move(record.meshSnap.after);
      if (IsEqual(m_undo.back().meshSnap.before,
                  m_undo.back().meshSnap.after)) {
        m_undo.pop_back();
      }
    } else {
      m_undo.back().overlaySnap.after = std::move(record.overlaySnap.after);
      if (IsEqual(m_undo.back().overlaySnap.before,
                  m_undo.back().overlaySnap.after)) {
        m_undo.pop_back();
      }
    }
  } else {
    m_undo.push_back(std::move(record));
  }
  return true;
}

// ---------------------------------------------------------------------------
const std::string &History::PeekUndoDesc() const {
  if (m_undo.empty())
    return s_emptyDesc;
  return (m_undo.back().type == EditType::Mesh)
             ? m_undo.back().meshSnap.description
             : m_undo.back().overlaySnap.description;
}

const std::string &History::PeekRedoDesc() const {
  if (m_redo.empty())
    return s_emptyDesc;
  return (m_redo.back().type == EditType::Mesh)
             ? m_redo.back().meshSnap.description
             : m_redo.back().overlaySnap.description;
}

// ---------------------------------------------------------------------------
void History::SetMaxDepth(int depth) {
  m_maxDepth = std::max(1, depth);
  while ((int)m_undo.size() > m_maxDepth)
    m_undo.pop_front();
}

// ---------------------------------------------------------------------------
void History::Clear() {
  m_undo.clear();
  m_redo.clear();
}
