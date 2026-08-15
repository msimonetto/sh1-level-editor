#pragma once
#include "raylib.h"

class Viewport;

// ---------------------------------------------------------------------------
// ViewportOverlay -- Base interface for visual layers injected into the Viewport
// ---------------------------------------------------------------------------
class ViewportOverlay {
public:
    virtual ~ViewportOverlay() = default;

    // Draw the overlay contents in 3D space
    virtual void DrawOverlay(Viewport& vp) = 0;

    // Handle picking logic from a mouse ray
    virtual void HandlePicking(Viewport& vp, Ray ray) {}

    // Handle picking logic from a 2D box selection
    virtual void HandleBoxPicking(Viewport& vp, Rectangle box) {}

    // Clean up all overlay-specific resources
    virtual void UnloadAll() {}

    // Draw context menu for the overlay (called within an ImGui Popup)
    virtual void DrawContextMenu() {}
};
