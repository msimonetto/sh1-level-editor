#pragma once
#include "raylib.h"
#include <string>
#include <vector>

class LocalGeometryOverlay;
class History;

namespace Geometry {

    // Rotates the active global object instance around its origin by yawSteps * 90 degrees.
    // yawSteps: +1 for +90 deg (CW), -1 for -90 deg (CCW), +2 for 180 deg.
    bool RotateGlobalObject(LocalGeometryOverlay& overlay, int yawSteps, History* history);

    // Snaps the active global object so its lowest point rests on Y = 0.0 floor.
    bool SnapGlobalObjectToFloor(LocalGeometryOverlay& overlay, History* history);

    // Snaps the active global object's XZ position to the active grid move step.
    bool SnapGlobalObjectToGrid(LocalGeometryOverlay& overlay, History* history);

    // Duplicates the currently selected global object instance with a position offset.
    bool DuplicateGlobalObject(LocalGeometryOverlay& overlay, History* history);

    // Deletes the currently selected global object instance from the chunk.
    bool DeleteGlobalObject(LocalGeometryOverlay& overlay, History* history);

    // Mirrors the global object across the specified axis (0=X, 1=Y, 2=Z).
    bool MirrorGlobalObject(LocalGeometryOverlay& overlay, int axis, History* history);

    // Moves the global object instance to another chunk, updating relative offsets.
    bool MoveGlobalObjectToChunk(LocalGeometryOverlay& overlay, const std::string& targetChunkName, History* history);

}
