#pragma once

class LocalGeometryOverlay;

// ---------------------------------------------------------------------------
// geometry/SubdivideFace — face subdivision for the LocalGeometryOverlay.
//
// Extracted from LocalGeometryOverlay::SubdivideSelectedFaces() for
// single-responsibility placement and potential future reuse.
// ---------------------------------------------------------------------------
namespace Geometry {

// Subdivide the currently selected faces in the given viewport into
// map-grid–sized tiles (1 world unit = 1 grid square = 256 raw IPD units).
// Respects the PS1 255-vertex mesh limit and records undo history.
void SubdivideSelectedFaces(LocalGeometryOverlay& vp);

} // namespace Geometry
