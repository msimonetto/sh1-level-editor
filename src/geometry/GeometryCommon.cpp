#include "geometry/GeometryCommon.h"
#include "viewport/LocalGeometryOverlay.h"
#include "formats/IPDParse.h"
#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <filesystem>

namespace Geometry {

std::string GetWorkspaceDir(const LocalGeometryOverlay& overlay) {
    if (std::filesystem::exists(overlay.m_lastWorkspaceDir))
        return overlay.m_lastWorkspaceDir;
    if (std::filesystem::exists("data/workspace"))
        return "data/workspace";
    if (std::filesystem::exists("../data/workspace"))
        return "../data/workspace";
    return overlay.m_lastWorkspaceDir;
}

float FindFloorHeightBelow(const LocalGeometryOverlay& overlay,
                           float worldX,
                           float worldZ,
                           float startY,
                           const std::string& ignoreChunk,
                           int ignoreObjIdx) {
    float highestFloorY = -99999.0f;
    Ray ray;
    ray.position = { worldX, startY + 5.0f, worldZ };
    ray.direction = { 0.0f, -1.0f, 0.0f };

    for (const auto& lc : overlay.GetChunks()) {
        if (!lc.visible || lc.hasError) continue;
        for (size_t oi = 0; oi < lc.data->objects.size(); ++oi) {
            if (lc.data->chunkName == ignoreChunk && (int)oi == ignoreObjIdx)
                continue;

            const auto& obj = lc.data->objects[oi];
            if (worldX < obj.bounds.min.x - 0.2f || worldX > obj.bounds.max.x + 0.2f ||
                worldZ < obj.bounds.min.z - 0.2f || worldZ > obj.bounds.max.z + 0.2f)
                continue;

            for (const auto& mesh : obj.meshes) {
                for (const auto& face : mesh.faces) {
                    bool isQuad = (face.v[3] != 0xFF);
                    int triCount = isQuad ? 2 : 1;
                    static const int triV[2][3] = {{0, 1, 2}, {0, 2, 3}};

                    for (int t = 0; t < triCount; ++t) {
                        Vector3 v1 = {mesh.vx[face.v[triV[t][0]]], mesh.vy[face.v[triV[t][0]]], mesh.vz[face.v[triV[t][0]]]};
                        Vector3 v2 = {mesh.vx[face.v[triV[t][1]]], mesh.vy[face.v[triV[t][1]]], mesh.vz[face.v[triV[t][1]]]};
                        Vector3 v3 = {mesh.vx[face.v[triV[t][2]]], mesh.vy[face.v[triV[t][2]]], mesh.vz[face.v[triV[t][2]]]};

                        RayCollision hit = GetRayCollisionTriangle(ray, v1, v2, v3);
                        if (hit.hit && hit.point.y <= startY + 0.1f && hit.point.y > highestFloorY) {
                            highestFloorY = hit.point.y;
                        }
                    }
                }
            }
        }
    }
    return highestFloorY;
}

void RecalculateObjectBounds(RenderObject& obj) {
    obj.bounds = {{99999.0f, 99999.0f, 99999.0f}, {-99999.0f, -99999.0f, -99999.0f}};
    for (const auto& mesh : obj.meshes) {
        for (size_t i = 0; i < mesh.vx.size(); ++i) {
            obj.bounds.min.x = std::min(obj.bounds.min.x, mesh.vx[i]);
            obj.bounds.min.y = std::min(obj.bounds.min.y, mesh.vy[i]);
            obj.bounds.min.z = std::min(obj.bounds.min.z, mesh.vz[i]);
            obj.bounds.max.x = std::max(obj.bounds.max.x, mesh.vx[i]);
            obj.bounds.max.y = std::max(obj.bounds.max.y, mesh.vy[i]);
            obj.bounds.max.z = std::max(obj.bounds.max.z, mesh.vz[i]);
        }
    }
}

} // namespace Geometry
