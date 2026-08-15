#pragma once
#include "raylib.h"
#include <string>
#include <vector>

class LocalGeometryOverlay;
class History;

namespace Geometry {

    // Triangulates all selected quad faces into two triangular faces.
    bool TriangulateFaces(LocalGeometryOverlay& overlay, History* history);

    // Connects / bridges two selected triangles into a single quad face if they share an edge.
    bool ConnectBridgeFaces(LocalGeometryOverlay& overlay, History* history);

    // Extrudes selected faces along their face normal.
    // mode: 0 = Extrude & Connect (creates connecting side quads), 1 = Separate (detaches face)
    bool ExtrudeFaces(LocalGeometryOverlay& overlay, float distance, int mode, History* history);

    // Inverts normals / face winding order of selected faces.
    bool InvertNormals(LocalGeometryOverlay& overlay, History* history);

    // Deletes selected faces.
    // deleteIsolatedVertices: if true, also removes any vertices left unreferenced.
    bool DeleteFaces(LocalGeometryOverlay& overlay, bool deleteIsolatedVertices, History* history);

    // --- UV & Texturing ---

    // Paints selected faces with the currently active tile from TextureMapPanel.
    bool PaintFaces(LocalGeometryOverlay& overlay, History* history);

    // Clears texture assignments from selected faces (sets to untextured).
    bool ClearTexture(LocalGeometryOverlay& overlay, History* history);

    // Rotates UV coordinates of selected faces by steps * 90 degrees (1=90, 2=180, 3=270).
    bool RotateUV(LocalGeometryOverlay& overlay, int steps, History* history);

    // Flips UV coordinates of selected faces horizontally or vertically.
    bool FlipUV(LocalGeometryOverlay& overlay, bool horizontal, bool vertical, History* history);

    // Fits UV coordinates of selected faces to standard tile bounds [0, 1].
    bool FitUVToTileBounds(LocalGeometryOverlay& overlay, History* history);

    // Resets UV coordinates of selected faces to default planar unit coordinates.
    bool ResetDefaultUV(LocalGeometryOverlay& overlay, History* history);

}
