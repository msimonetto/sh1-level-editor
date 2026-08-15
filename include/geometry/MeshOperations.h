#pragma once
#include "raylib.h"
#include "core/IPDParse.h"
#include "viewport/LocalGeometry.h"
#include <vector>
#include <set>
#include <string>
#include <tuple>

class History;

namespace Geometry {

    // Centralized operation for translating selected vertices, faces, or entire objects.
    bool TranslateSelection(
        Vector3 delta,
        EditMode editMode,
        const std::string& selectedChunk,
        int selectedObjectIdx,
        const std::set<SelectedVertex>& selectedVertices,
        const std::set<SelectedFace>& selectedFaces,
        std::vector<LoadedChunk>& chunks,
        bool autoValidate,
        History* history,
        const std::string& workspaceDir
    );

    // Rotates the active local mesh or object by angleDeg around the given axis (0=X, 1=Y, 2=Z).
    bool RotateMesh(LocalGeometryOverlay& overlay, int axis, float angleDeg, History* history);

    // Mirrors the active local mesh across the given axis (0=X, 1=Y, 2=Z) and inverts winding.
    bool MirrorMesh(LocalGeometryOverlay& overlay, int axis, History* history);

    // Snaps the active local mesh so its lowest point rests on the floor / geometry below.
    bool SnapMeshToFloor(LocalGeometryOverlay& overlay, History* history);

    // Duplicates the active local mesh within the current object or creates a new mesh.
    bool DuplicateMesh(LocalGeometryOverlay& overlay, History* history);

    // Separates the selected local mesh from its parent object into a brand new RenderObject.
    bool SeparateMeshToNewObject(LocalGeometryOverlay& overlay, History* history);

    // Merges all meshes in the active object into a single combined RenderMesh.
    bool MergeMeshes(LocalGeometryOverlay& overlay, History* history);

    // Deletes the active local mesh.
    bool DeleteMesh(LocalGeometryOverlay& overlay, History* history);

    // Recalculates tight AABB bounding boxes for all meshes and objects in the active chunk.
    bool RecalculateBounds(LocalGeometryOverlay& overlay);

    // Centers the active mesh vertices around the origin/centroid.
    bool CenterMeshPivot(LocalGeometryOverlay& overlay, History* history);

    // Adds a low-poly PS1 primitive mesh (0=Floor Plane, 1=Wall Plane, 2=Cube Block, 3=Ramp/Slope).
    bool AddPrimitive(LocalGeometryOverlay& overlay, int primType, float width, float height, float length, History* history);

    // Moves the active local mesh to another chunk in the workspace.
    bool MoveMeshToChunk(LocalGeometryOverlay& overlay, const std::string& targetChunkName, History* history);

}
