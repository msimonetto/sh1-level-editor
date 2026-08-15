#pragma once

#include "core/Shortcuts.h"
#include "core/ChunkManager.h"
#include "viewport/Viewport.h"
#include "core/History.h"
#include "viewport/LocalGeometry.h"
#include "viewport/Waypoints.h"
#include "panels/Settings.h"

namespace MenuPanel {
    bool Draw(Shortcuts& shortcuts, ChunkManager& pipelineManager, Viewport& viewport, History& history, LocalGeometryOverlay& localGeometryOverlay, WaypointsOverlay& eventOverlay, SettingsPanel& settingsWindow);
}

