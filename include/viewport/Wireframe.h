#pragma once
#include <set>
#include <vector>
#include "raylib.h"
#include "rlgl.h"
#include "viewport/ViewportBase.h"  // LoadedChunk
#include "viewport/Frustum.h"
#include "core/IPDParse.h"

// Forward declarations
struct SelectedFace;
struct SelectedVertex;
enum class EditMode;

// ---------------------------------------------------------------------------
// Wireframe — shared wireframe rendering helpers for 3D viewports.
//
// These free functions operate on generic loaded-chunk data so they can be
// reused across LocalGeometry, Collision, GlobalGeometry, and Scene viewports.
// ---------------------------------------------------------------------------
namespace Wireframe {

struct Config {
    bool showPersistent;
    float lineWidth;
    Color baseColor;
};

// Draw face-outline wireframes for all visible objects (persistent mode).
// Skips global objects when editMode is Face or Vertex.
void DrawPersistentOverlay(
    const std::vector<LoadedChunk>& chunks,
    const Frustum& frustum,
    EditMode editMode,
    bool showPersistentWireframe,
    float wireframeThickness,
    Color wireframeColor);

// Draw the wireframe for a single mesh.
void DrawMeshWireframe(
    const RenderMesh& mesh, 
    Color wireframeColor, 
    float wireframeThickness);

// Draw the wireframe for an entire object (all meshes).
void DrawObjectWireframe(
    const RenderObject& obj, 
    Color wireframeColor, 
    float wireframeThickness);

// Draw yellow outlines around all currently selected faces.
void DrawSelectedFaceOutlines(
    const std::set<SelectedFace>& selectedFaces,
    const std::vector<LoadedChunk>& chunks);

// Draw vertex dots (hover proximity + selected) in Vertex edit mode.
// Returns the index/chunk of the closest hovered vertex (or -1 if none).
void DrawVertexOverlay(
    const std::vector<LoadedChunk>& chunks,
    const std::set<SelectedVertex>& selectedVertices,
    bool hovered,
    const Camera3D& camera,
    int rtWidth,
    int rtHeight,
    Vector2 localMousePos,
    const Frustum& frustum);

} // namespace Wireframe
