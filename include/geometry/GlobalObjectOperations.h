#pragma once
#include "raylib.h"
#include <string>
#include <vector>

class LocalGeometryOverlay;
class History;

namespace Geometry {

    // --- Inspection & Rotation ---

    // Reads the current Euler angles (pitch, yaw, roll in degrees) of the active global object.
    bool GetGlobalObjectRotation(const LocalGeometryOverlay& overlay, float& pitch, float& yaw, float& roll);

    // Sets the internal Euler orientation (pitch, yaw, roll in degrees) from GUI inputs.
    bool SetGlobalObjectRotation(LocalGeometryOverlay& overlay, float pitch, float yaw, float roll, History* history = nullptr);

    // Rotates the active global object around an external world axis (X, Y, Z) by angleRad at its world anchor.
    bool RotateGlobalObjectExternal(LocalGeometryOverlay& overlay, Vector3 worldAxis, float angleRad, History* history = nullptr);

    // Rotates the active global object instance around its origin by yawSteps * 90 degrees (+1 = +90°, -1 = -90°, +2 = 180°).
    bool RotateGlobalObject(LocalGeometryOverlay& overlay, int yawSteps, History* history = nullptr);

    // --- Translation & Snapping ---

    // Translates the active global object by a world offset vector.
    bool TranslateGlobalObject(LocalGeometryOverlay& overlay, Vector3 worldDelta, History* history = nullptr);

    // Snaps the active global object so its lowest point rests on the floor below it.
    bool SnapGlobalObjectToFloor(LocalGeometryOverlay& overlay, History* history = nullptr);

    // Snaps the active global object's XZ position to the active grid move step.
    bool SnapGlobalObjectToGrid(LocalGeometryOverlay& overlay, History* history = nullptr);

    // --- Lifecycle & Management ---

    // Duplicates the currently selected global object instance with a position offset.
    bool DuplicateGlobalObject(LocalGeometryOverlay& overlay, History* history = nullptr);

    // Deletes the currently selected global object instance from the chunk.
    bool DeleteGlobalObject(LocalGeometryOverlay& overlay, History* history = nullptr);

    // Mirrors the global object across the specified axis (0=X, 1=Y, 2=Z).
    bool MirrorGlobalObject(LocalGeometryOverlay& overlay, int axis, History* history = nullptr);

    // Moves the global object instance to another chunk, updating relative offsets.
    bool MoveGlobalObjectToChunk(LocalGeometryOverlay& overlay, const std::string& targetChunkName, History* history = nullptr);

}

