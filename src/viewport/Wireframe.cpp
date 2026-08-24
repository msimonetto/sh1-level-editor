#include "viewport/Wireframe.h"
#include "viewport/LocalGeometryOverlay.h"  // SelectedFace, SelectedVertex, EditMode
#include "raymath.h"
#include "rlgl.h"

namespace Wireframe {

// ---------------------------------------------------------------------------
// DrawPersistentOverlay
// Draws face-edge wireframes for every visible object across all chunks.
// Global objects receive a red-tinted colour when in Face/Vertex mode.
// ---------------------------------------------------------------------------
void DrawPersistentOverlay(
    const std::vector<LoadedChunk>& chunks,
    const Frustum& frustum,
    EditMode editMode,
    bool showPersistentWireframe,
    float wireframeThickness,
    Color wireframeColor)
{
    if (!showPersistentWireframe)
        return;

    rlDrawRenderBatchActive();
    rlSetLineWidth(wireframeThickness);

    for (const auto& lc : chunks) {
        if (!lc.visible || lc.hasError)
            continue;
        if (!frustum.IsBoxInFrustum(lc.bounds))
            continue;

        for (size_t objIdx = 0; objIdx < lc.data->objects.size(); ++objIdx) {
            const auto& obj = lc.data->objects[objIdx];
            if (!frustum.IsBoxInFrustum(obj.bounds))
                continue;

            Color wireColor = wireframeColor;
            if (obj.isGlobal &&
                (editMode == EditMode::Face || editMode == EditMode::Vertex)) {
                wireColor = Color{240, 90, 80, wireframeColor.a};
            }

            for (const auto& mesh : obj.meshes) {
                for (const auto& face : mesh.faces) {
                    bool isQuad = (face.v[3] != 0xFF);
                    Vector3 v0 = {mesh.vx[face.v[0]], mesh.vy[face.v[0]], mesh.vz[face.v[0]]};
                    Vector3 v1 = {mesh.vx[face.v[1]], mesh.vy[face.v[1]], mesh.vz[face.v[1]]};
                    Vector3 v2 = {mesh.vx[face.v[2]], mesh.vy[face.v[2]], mesh.vz[face.v[2]]};

                    if (isQuad) {
                        Vector3 v3 = {mesh.vx[face.v[3]], mesh.vy[face.v[3]], mesh.vz[face.v[3]]};
                        DrawLine3D(v0, v1, wireColor);
                        DrawLine3D(v1, v2, wireColor);
                        DrawLine3D(v2, v3, wireColor);
                        DrawLine3D(v3, v0, wireColor);
                    } else {
                        DrawLine3D(v0, v1, wireColor);
                        DrawLine3D(v1, v2, wireColor);
                        DrawLine3D(v2, v0, wireColor);
                    }
                }
            }
        }
    }

    rlDrawRenderBatchActive();
    rlSetLineWidth(1.0f);
}

// ---------------------------------------------------------------------------
// DrawMeshWireframe
// Draws the wireframe for a single mesh.
// ---------------------------------------------------------------------------
void DrawMeshWireframe(
    const RenderMesh& mesh, 
    Color wireframeColor, 
    float wireframeThickness)
{
    rlDrawRenderBatchActive();
    rlSetLineWidth(wireframeThickness);

    for (const auto& face : mesh.faces) {
        bool isQuad = (face.v[3] != 0xFF);
        Vector3 v0 = {mesh.vx[face.v[0]], mesh.vy[face.v[0]], mesh.vz[face.v[0]]};
        Vector3 v1 = {mesh.vx[face.v[1]], mesh.vy[face.v[1]], mesh.vz[face.v[1]]};
        Vector3 v2 = {mesh.vx[face.v[2]], mesh.vy[face.v[2]], mesh.vz[face.v[2]]};

        if (isQuad) {
            Vector3 v3 = {mesh.vx[face.v[3]], mesh.vy[face.v[3]], mesh.vz[face.v[3]]};
            DrawLine3D(v0, v1, wireframeColor);
            DrawLine3D(v1, v2, wireframeColor);
            DrawLine3D(v2, v3, wireframeColor);
            DrawLine3D(v3, v0, wireframeColor);
        } else {
            DrawLine3D(v0, v1, wireframeColor);
            DrawLine3D(v1, v2, wireframeColor);
            DrawLine3D(v2, v0, wireframeColor);
        }
    }

    rlDrawRenderBatchActive();
    rlSetLineWidth(1.0f);
}

// ---------------------------------------------------------------------------
// DrawObjectWireframe
// Draws the wireframe for an entire object (all meshes).
// ---------------------------------------------------------------------------
void DrawObjectWireframe(
    const RenderObject& obj, 
    Color wireframeColor, 
    float wireframeThickness)
{
    for (const auto& mesh : obj.meshes) {
        DrawMeshWireframe(mesh, wireframeColor, wireframeThickness);
    }
}

// ---------------------------------------------------------------------------
// DrawSelectedFaceOutlines
// Draws YELLOW outlines around every face in the selection set.
// Called inside BeginMode3D with depth test disabled by the caller.
// ---------------------------------------------------------------------------
void DrawSelectedFaceOutlines(
    const std::set<SelectedFace>& selectedFaces,
    const std::vector<LoadedChunk>& chunks)
{
    if (selectedFaces.empty())
        return;

    rlDisableDepthTest();
    rlDrawRenderBatchActive();

    for (const auto& sf : selectedFaces) {
        for (const auto& lc : chunks) {
            if (lc.data->chunkName == sf.chunkName) {
                if (sf.objectIdx < (int)lc.data->objects.size()) {
                    const auto& obj = lc.data->objects[sf.objectIdx];
                    if (sf.meshIdx < (int)obj.meshes.size()) {
                        const auto& mesh = obj.meshes[sf.meshIdx];
                        if (sf.faceIdx < (int)mesh.faces.size()) {
                            const auto& face = mesh.faces[sf.faceIdx];
                            bool isQuad = (face.v[3] != 0xFF);

                            Vector3 v1 = {mesh.vx[face.v[0]], mesh.vy[face.v[0]], mesh.vz[face.v[0]]};
                            Vector3 v2 = {mesh.vx[face.v[1]], mesh.vy[face.v[1]], mesh.vz[face.v[1]]};
                            Vector3 v3 = {mesh.vx[face.v[2]], mesh.vy[face.v[2]], mesh.vz[face.v[2]]};

                            if (isQuad) {
                                Vector3 v4 = {mesh.vx[face.v[3]], mesh.vy[face.v[3]], mesh.vz[face.v[3]]};
                                DrawLine3D(v1, v2, YELLOW);
                                DrawLine3D(v2, v3, YELLOW);
                                DrawLine3D(v3, v4, YELLOW);
                                DrawLine3D(v4, v1, YELLOW);
                            } else {
                                DrawLine3D(v1, v2, YELLOW);
                                DrawLine3D(v2, v3, YELLOW);
                                DrawLine3D(v3, v1, YELLOW);
                            }
                        }
                    }
                }
                break;
            }
        }
    }

    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

// ---------------------------------------------------------------------------
// DrawVertexOverlay
// Draws magenta dots for all nearby vertices (proximity), green cubes for
// selected vertices, and a larger magenta cube for the closest hovered vertex.
// ---------------------------------------------------------------------------
void DrawVertexOverlay(
    const std::vector<LoadedChunk>& chunks,
    const std::set<SelectedVertex>& selectedVertices,
    bool hovered,
    const Camera3D& camera,
    int rtWidth,
    int rtHeight,
    Vector2 localMousePos,
    const Frustum& frustum)
{
    if (!hovered)
        return;

    rlDisableDepthTest();

    // --- Find closest vertex (hover) ---
    int closestIdx = -1;
    int closestMeshIdx = -1;
    int closestObjIdx = -1;
    std::string closestChunk;
    float bestDist2D = 100.0f; // pixel radius

    Vector3 forward = Vector3Normalize({
        camera.target.x - camera.position.x,
        camera.target.y - camera.position.y,
        camera.target.z - camera.position.z});

    for (const auto& lc : chunks) {
        if (!lc.visible || lc.hasError)
            continue;
        if (!frustum.IsBoxInFrustum(lc.bounds))
            continue;

        for (size_t objIdx = 0; objIdx < lc.data->objects.size(); ++objIdx) {
            const auto& obj = lc.data->objects[objIdx];
            if (obj.bounds.min.x > obj.bounds.max.x)
                continue;
            if (obj.isGlobal)
                continue;
            if (!frustum.IsBoxInFrustum(obj.bounds))
                continue;

            for (size_t meshIdx = 0; meshIdx < obj.meshes.size(); ++meshIdx) {
                const auto& mesh = obj.meshes[meshIdx];
                for (size_t vi = 0; vi < mesh.vx.size(); ++vi) {
                    Vector3 v = {mesh.vx[vi], mesh.vy[vi], mesh.vz[vi]};
                    Vector3 toV = {v.x - camera.position.x, v.y - camera.position.y, v.z - camera.position.z};

                    if (Vector3DotProduct(forward, toV) > 0.1f) {
                        Vector2 screenPos = GetWorldToScreenEx(v, camera, rtWidth, rtHeight);
                        float dist2D = Vector2Distance(screenPos, localMousePos);
                        if (dist2D < bestDist2D) {
                            bestDist2D = dist2D;
                            closestIdx = (int)vi;
                            closestMeshIdx = (int)meshIdx;
                            closestObjIdx = (int)objIdx;
                            closestChunk = lc.data->chunkName;
                        }
                    }
                }
            }
        }
    }

    // --- Draw small magenta cubes for nearby, non-selected vertices ---
    for (const auto& lc : chunks) {
        if (!lc.visible || lc.hasError)
            continue;
        if (!frustum.IsBoxInFrustum(lc.bounds))
            continue;

        for (size_t objIdx = 0; objIdx < lc.data->objects.size(); ++objIdx) {
            const auto& obj = lc.data->objects[objIdx];
            if (obj.bounds.min.x > obj.bounds.max.x)
                continue;
            if (obj.isGlobal)
                continue;
            if (!frustum.IsBoxInFrustum(obj.bounds))
                continue;

            for (size_t meshIdx = 0; meshIdx < obj.meshes.size(); ++meshIdx) {
                const auto& mesh = obj.meshes[meshIdx];
                for (size_t vi = 0; vi < mesh.vx.size(); ++vi) {
                    // Skip selected verts — drawn separately in green
                    SelectedVertex svSearch = {lc.data->chunkName, (int)objIdx, (int)meshIdx, (int)vi};
                    if (selectedVertices.find(svSearch) != selectedVertices.end())
                        continue;
                    // Skip the closest hovered vert — drawn separately below
                    if (closestChunk == lc.data->chunkName &&
                        closestObjIdx == (int)objIdx &&
                        closestMeshIdx == (int)meshIdx &&
                        closestIdx == (int)vi)
                        continue;

                    Vector3 v = {mesh.vx[vi], mesh.vy[vi], mesh.vz[vi]};
                    Vector3 toV = {v.x - camera.position.x, v.y - camera.position.y, v.z - camera.position.z};
                    if (Vector3DotProduct(forward, toV) > 0.1f) {
                        Vector2 screenPos = GetWorldToScreenEx(v, camera, rtWidth, rtHeight);
                        if (Vector2Distance(screenPos, localMousePos) < 100.0f) {
                            DrawCube(v, 0.02f, 0.02f, 0.02f, MAGENTA);
                        }
                    }
                }
            }
        }
    }

    // --- Draw closest hovered vertex (larger) ---
    if (closestIdx >= 0) {
        SelectedVertex svSearch = {closestChunk, closestObjIdx, closestMeshIdx, closestIdx};
        if (selectedVertices.find(svSearch) == selectedVertices.end()) {
            for (const auto& lc : chunks) {
                if (lc.data->chunkName == closestChunk &&
                    closestObjIdx < (int)lc.data->objects.size()) {
                    const auto& obj = lc.data->objects[closestObjIdx];
                    if (closestMeshIdx < (int)obj.meshes.size()) {
                        const auto& mesh = obj.meshes[closestMeshIdx];
                        Vector3 v = {mesh.vx[closestIdx], mesh.vy[closestIdx], mesh.vz[closestIdx]};
                        DrawCube(v, 0.05f, 0.05f, 0.05f, MAGENTA);
                    }
                    break;
                }
            }
        }
    }

    rlEnableDepthTest();
}

} // namespace Wireframe
