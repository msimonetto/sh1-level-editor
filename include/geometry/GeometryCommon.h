#pragma once
#include "raylib.h"
#include <cstdint>
#include <string>
#include <set>
#include <vector>
#include <functional>

class LocalGeometryOverlay;
class History;
struct LoadedChunk;
struct RenderObject;
struct RenderMesh;
struct RenderFace;
struct SelectedVertex;
struct SelectedFace;

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

// Resolve effective multi-selection vs single active selection
std::set<SelectedVertex> GetEffectiveSelectedVertices(const LocalGeometryOverlay& overlay);
std::set<SelectedFace>   GetEffectiveSelectedFaces(const LocalGeometryOverlay& overlay);

// Resolve the active LoadedChunk, RenderObject, and RenderMesh target from overlay selection
bool GetActiveMeshTarget(const LocalGeometryOverlay& overlay,
                         LoadedChunk*& outLc,
                         RenderObject*& outObj,
                         RenderMesh*& outMesh);

// Mesh vertex access and manipulation helpers
Vector3 GetMeshVertex(const RenderMesh& mesh, size_t idx);
uint8_t AddMeshVertex(RenderMesh& mesh, Vector3 pos);
Vector3 ComputeVertexCentroid(const RenderMesh& mesh, const std::vector<int>& indices);
std::vector<uint8_t> SortVerticesByAngle(const RenderMesh& mesh, const std::vector<int>& vertIndices);

// Remove unreferenced vertices from mesh buffers and remap all face vertex indices
void CompactMeshUnusedVertices(RenderMesh& mesh);

// Construct a default textured or untextured quad/triangle face with canonical UVs
RenderFace CreateDefaultFace(const RenderObject& obj,
                             int meshIdx,
                             const std::vector<uint8_t>& vertIndices,
                             const RenderFace* inheritFace = nullptr);

// Callback signatures for per-mesh operations
using VertexMeshOp = std::function<bool(LoadedChunk& lc, RenderObject& obj, RenderMesh& mesh, const std::vector<int>& vertIndices)>;
using FaceMeshOp   = std::function<bool(LoadedChunk& lc, RenderObject& obj, RenderMesh& mesh, const std::vector<int>& faceIndices)>;

// Surface normal computation for a render face
Vector3 ComputeFaceNormal(const RenderMesh& mesh, const RenderFace& face);

// Generic iteration across all unique meshes involved in active vertex selection
bool ForEachSelectedMeshVertices(
    LocalGeometryOverlay& overlay,
    History* history,
    const char* actionDescription,
    const VertexMeshOp& op);

// Generic iteration across all unique meshes involved in active face selection
bool ForEachSelectedMeshFaces(
    LocalGeometryOverlay& overlay,
    History* history,
    const char* actionDescription,
    const FaceMeshOp& op,
    bool clearSelection = false,
    bool recalculateBounds = false);

} // namespace Geometry

