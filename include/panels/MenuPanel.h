#pragma once

#include "core/Shortcuts.h"
#include "core/FileManager.h"
#include "viewport/Viewport.h"
#include "core/History.h"
#include "viewport/LocalGeometryOverlay.h"
#include "viewport/WaypointsOverlay.h"
#include "panels/SettingsPanel.h"

namespace MenuPanel {
    bool Draw(Shortcuts& shortcuts, FileManager& fileManager, Viewport& viewport, History& history, LocalGeometryOverlay& localGeometryOverlay, WaypointsOverlay& eventOverlay, SettingsPanel& settingsWindow);
}