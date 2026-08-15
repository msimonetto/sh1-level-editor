#pragma once
#include "raylib.h"
#include <string>
#include <vector>

class LocalGeometryOverlay;
class History;

namespace Geometry {

    // Snaps all selected vertices to the active power-of-two grid step.
    bool SnapVerticesToGrid(LocalGeometryOverlay& overlay, History* history);

    // Snaps all selected vertices to the floor or geometry below them (or Y=0.0).
    bool SnapVerticesToFloor(LocalGeometryOverlay& overlay, History* history);

    // Planarizes / flattens selected vertices along the specified axis (0=X, 1=Y, 2=Z) to their average coordinate.
    bool PlanarizeVertices(LocalGeometryOverlay& overlay, int axis, History* history);

    // Creates a new face (triangle for 3 vertices, quad for 4 vertices) from selected vertices.
    bool AddFaceFromSelectedVertices(LocalGeometryOverlay& overlay, History* history);

    // Extrudes selected vertices or edges by the specified offset vector, creating connecting quad faces if edges exist.
    bool ExtrudeSelectedVertices(LocalGeometryOverlay& overlay, Vector3 offset, History* history);

    // Welds / merges coincident vertices within tolerance distance into single vertices.
    bool WeldVertices(LocalGeometryOverlay& overlay, float tolerance, History* history);

    // Deletes selected vertices and removes any attached faces from the mesh.
    bool DeleteSelectedVertices(LocalGeometryOverlay& overlay, History* history);

}
