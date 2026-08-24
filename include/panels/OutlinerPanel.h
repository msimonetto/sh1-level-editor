#pragma once
#include "viewport/Viewport.h"

class LocalGeometryOverlay;

// ---------------------------------------------------------------------------
// OutlinerPanel - read-only ImGui tree of loaded chunks.
//
// Analogous to Blender's Outliner with one collection per chunk.
// Architecture hook for future object selection, property editing, etc.
//
// Usage: OutlinerPanel outliner; outliner.Draw(viewport,
// &localGeometryViewport);
// ---------------------------------------------------------------------------
class OutlinerPanel {
public:
  // Draw the ImGui panel, referencing the viewport's chunk list.
  // localGeometryViewport (optional) is used to select faces/vertices when an
  // object is clicked.
  void Draw(Viewport &viewport,
            LocalGeometryOverlay *localGeometryOverlay = nullptr);
};

