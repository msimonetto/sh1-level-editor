#pragma once
#include "formats/OverlayLoader.h"
#include "viewport/ViewportBase.h"
#include "viewport/ViewportOverlay.h"
class Viewport;
#include "raylib.h"
#include <map>
#include <string>
#include <vector>


class SceneOverlay;

// ---------------------------------------------------------------------------
// WaypointsOverlay -- Viewport for Door Links, Waypoints, and Event Triggers
// ---------------------------------------------------------------------------
class WaypointsOverlay : public ViewportOverlay {
public:
  WaypointsOverlay();
  ~WaypointsOverlay();

  // Set shared chunk geometry pointer from Viewport (zero extra VRAM)
  void SetSharedChunks(const std::vector<LoadedChunk> *sharedChunks) {
    m_sharedChunks = sharedChunks;
  }

  // Load overlay data for active map key (e.g. "MAP0_S00")
  bool LoadOverlay(const std::string &mapKey);

  // Auto-focus camera on the loaded overlay geometry
  void FocusOnOverlay(Viewport &vp);

  const OverlayMapData &GetOverlay() const { return m_overlay; }
  OverlayMapData &GetOverlay() { return m_overlay; }

  int GetSelectedWaypointIdx() const { return m_selectedWaypointIdx; }
  int GetSelectedLinkIdx() const { return m_selectedLinkIdx; }

  void SetSelectedWaypoint(int idx) { m_selectedWaypointIdx = idx; }
  void SetSelectedLink(int idx) { m_selectedLinkIdx = idx; }

  // Layer & Relationship Visibility Toggles
  bool m_showWaypoints = true;
  bool m_showTriggers = true;
  bool m_showDirectionLines = true;
  bool m_showBackgroundGeo = true;
  bool m_showLabels = true;
  bool m_showAllRelationships =
      true; // Checkbox toggle in Tools panel to draw all door link beams

  void DrawOverlay(Viewport &vp) override;
  void HandlePicking(Viewport &vp, Ray ray) override;
  void DrawContextMenu() override;

private:
  const std::vector<LoadedChunk> *m_sharedChunks = nullptr;
  OverlayMapData m_overlay;
  std::map<std::string, OverlayMapData> m_overlayCache;

  int m_selectedWaypointIdx = -1;
  int m_selectedLinkIdx = -1;

  void DrawBackgroundGeometry();
  void DrawWaypoints();
  void DrawTriggers();
  void DrawDoorFrame(Vector3 center, float angleDeg, float width, float height,
                     Color color, bool isSelected);
  void DrawDirectionLines();
  void DrawRelationshipBeams();

  bool GetTargetWaypointPos(const LinkData &link, Vector3 &outPos,
                            std::string &outMapLabel);
  Color GetEventColor(const LinkData &link) const;
};
