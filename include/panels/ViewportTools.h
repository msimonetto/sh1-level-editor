#pragma once
#include "panels/LocalGeometry.h"
#include "panels/Waypoints.h"
#include <string>

class ViewportBase;
class History;

enum class ViewportMode {
  Scene,
  LocalGeometry,
  Collision,
  DoorsAndWaypoints
};

// ---------------------------------------------------------------------------
// ViewportToolsPanel — Conduit container for viewport mode tools.
//
// Hosts the top mode tab bar (Scene, Local Geometry, Collision, Waypoints),
// tracks the active viewport, and delegates content rendering to the respective
// sub-panels (LocalGeometryPanel, WaypointsPanel, etc.).
// ---------------------------------------------------------------------------
class ViewportToolsPanel {
public:
  ViewportToolsPanel();
  ~ViewportToolsPanel() = default;

  void Draw(History *editHistory = nullptr);

  // Link active viewport
  void SetActiveViewport(ViewportBase *viewport);

  ViewportMode GetActiveMode() const { return m_activeMode; }
  void SetActiveMode(ViewportMode mode) {
    m_activeMode = mode;
    m_forceTabSelection = true;
  }

private:
  bool m_forceTabSelection = false;
  ViewportBase *m_activeViewport = nullptr;
  ViewportMode m_activeMode = ViewportMode::Scene;

  // Sub-panel tool inspectors
  LocalGeometryPanel m_localGeometryPanel;
  WaypointsPanel m_waypointsPanel;
};
