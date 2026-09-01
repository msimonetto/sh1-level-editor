#pragma once
#include <string>

class LocalGeometryOverlay;
struct RenderObject;

namespace Geometry {

// Resolve the active workspace directory from overlay state or standard fallbacks
std::string GetWorkspaceDir(const LocalGeometryOverlay& overlay);

// Downward raycast across chunk ground geometry to determine floor height at (worldX, worldZ)
float FindFloorHeightBelow(const LocalGeometryOverlay& overlay,
                           float worldX,
                           float worldZ,
                           float startY,
                           const std::string& ignoreChunk = "",
                           int ignoreObjIdx = -1);

// Recalculate object-level 3D AABB bounds from all constituent mesh vertices
void RecalculateObjectBounds(RenderObject& obj);

} // namespace Geometry
