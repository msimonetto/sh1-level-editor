#include "geometry/ChunkOwnership.h"
#include "geometry/GeometryCommon.h"
#include "viewport/LocalGeometryOverlay.h"
#include "core/History.h"
#include <cmath>
#include <map>
#include <algorithm>

namespace Geometry {

bool ChunkGridCell::operator==(const ChunkGridCell& o) const {
    return x == o.x && y == o.y;
}

bool ChunkGridCell::operator!=(const ChunkGridCell& o) const {
    return !(*this == o);
}

bool ChunkGridCell::operator<(const ChunkGridCell& o) const {
    if (x != o.x) return x < o.x;
    return y < o.y;
}

bool ChunkOccupancy::IsOwnedBy(int8_t xPos, int8_t yPos) const {
    return occupiedCells.size() == 1 && occupiedCells[0].x == xPos && occupiedCells[0].y == yPos;
}

static std::string ResolveChunkOwner(ChunkGridCell cell, const std::string& prefix, const std::vector<ParsedChunk>* loadedChunks) {
    if (loadedChunks) {
        for (const auto& c : *loadedChunks) {
            if (c.xPos == cell.x && c.yPos == cell.y) {
                return c.chunkName;
            }
        }
    }
    return Core::FormatChunkName(prefix, cell.x, cell.y);
}

static bool ResolveChunkPair(LocalGeometryOverlay& overlay, const std::string& targetChunkName, LoadedChunk*& outSrc, LoadedChunk*& outDst) {
    if (overlay.m_selectedChunk.empty() || targetChunkName.empty() || targetChunkName == overlay.m_selectedChunk) {
        return false;
    }
    outSrc = nullptr;
    outDst = nullptr;
    for (auto& lcConst : overlay.GetChunks()) {
        auto& lc = const_cast<LoadedChunk&>(lcConst);
        if (lc.data && lc.data->chunkName == overlay.m_selectedChunk) outSrc = &lc;
        if (lc.data && lc.data->chunkName == targetChunkName) outDst = &lc;
    }
    return (outSrc && outDst);
}

ChunkGridCell ChunkOwnership::WorldToGridPos(Vector3 worldPos) {
    return {
        static_cast<int8_t>(std::floor(worldPos.x / GRID_SIZE)),
        static_cast<int8_t>(std::floor(-worldPos.z / GRID_SIZE))
    };
}

BoundingBox ChunkOwnership::GridToWorldBounds(int8_t xPos, int8_t yPos, bool includeOverhang) {
    float overhang = includeOverhang ? OVERHANG_WORLD : 0.0f;
    return {
        { static_cast<float>(xPos) * GRID_SIZE - overhang,        -16.0f, -(static_cast<float>(yPos) + 1.0f) * GRID_SIZE - overhang },
        { (static_cast<float>(xPos) + 1.0f) * GRID_SIZE + overhang, 16.0f, -(static_cast<float>(yPos)) * GRID_SIZE + overhang }
    };
}

Vector3 ChunkOwnership::GridToWorldCenter(int8_t xPos, int8_t yPos) {
    return {
        static_cast<float>(xPos) * GRID_SIZE + (GRID_SIZE * 0.5f),
        0.0f,
        -(static_cast<float>(yPos) * GRID_SIZE + (GRID_SIZE * 0.5f))
    };
}

std::string ChunkOwnership::FormatChunkName(const std::string& prefix, int8_t gx, int8_t gy) {
    return Core::FormatChunkName(prefix, gx, gy);
}

bool ChunkOwnership::ParseChunkName(const std::string& chunkName, std::string& outPrefix, int8_t& outX, int8_t& outY) {
    return Core::ParseChunkName(chunkName, outPrefix, outX, outY);
}

std::string ChunkOwnership::ExtractChunkPrefix(const std::string& chunkName) {
    return Core::ExtractChunkPrefix(chunkName);
}

ChunkOccupancy ChunkOwnership::AnalyzeMeshOccupancy(const RenderMesh& mesh, 
                                                    int8_t currentChunkX, 
                                                    int8_t currentChunkY, 
                                                    const std::string& prefix) {
    ChunkOccupancy occ;
    if (mesh.vx.empty()) {
        occ.dominantCell = { currentChunkX, currentChunkY };
        occ.occupiedCells.push_back(occ.dominantCell);
        if (!prefix.empty()) {
            occ.suggestedChunkName = Core::FormatChunkName(prefix, currentChunkX, currentChunkY);
        }
        return occ;
    }

    std::map<ChunkGridCell, int> cellCounts;
    for (size_t i = 0; i < mesh.vx.size(); ++i) {
        cellCounts[WorldToGridPos(GetMeshVertex(mesh, i))]++;
    }

    int maxCount = -1;
    for (const auto& kv : cellCounts) {
        occ.occupiedCells.push_back(kv.first);
        if (kv.second > maxCount) {
            maxCount = kv.second;
            occ.dominantCell = kv.first;
        }
    }

    occ.spansMultipleChunks = (occ.occupiedCells.size() > 1);
    if (!prefix.empty()) {
        occ.suggestedChunkName = Core::FormatChunkName(prefix, occ.dominantCell.x, occ.dominantCell.y);
    }

    return occ;
}

std::string ChunkOwnership::DetermineMeshOwner(const RenderMesh& mesh, 
                                               int8_t currentChunkX, 
                                               int8_t currentChunkY, 
                                               const std::string& prefix, 
                                               const std::vector<ParsedChunk>* loadedChunks) {
    ChunkOccupancy occ = AnalyzeMeshOccupancy(mesh, currentChunkX, currentChunkY, prefix);
    return ResolveChunkOwner(occ.dominantCell, prefix, loadedChunks);
}

std::string ChunkOwnership::DetermineObjectOwner(const RenderObject& obj, 
                                                 int8_t currentChunkX, 
                                                 int8_t currentChunkY, 
                                                 const std::string& prefix, 
                                                 const std::vector<ParsedChunk>* loadedChunks) {
    if (obj.isGlobal) {
        float worldX = (static_cast<float>(obj.rawTx) + 10240.0f * static_cast<float>(currentChunkX)) * (1.0f / 256.0f);
        float worldZ = -(static_cast<float>(obj.rawTz) + 10240.0f * static_cast<float>(currentChunkY)) * (1.0f / 256.0f);
        return ResolveChunkOwner(WorldToGridPos({ worldX, 0.0f, worldZ }), prefix, loadedChunks);
    }

    std::map<ChunkGridCell, int> totalCellCounts;
    for (const auto& mesh : obj.meshes) {
        for (size_t i = 0; i < mesh.vx.size(); ++i) {
            totalCellCounts[WorldToGridPos(GetMeshVertex(mesh, i))]++;
        }
    }

    if (totalCellCounts.empty()) {
        return ResolveChunkOwner({ currentChunkX, currentChunkY }, prefix, loadedChunks);
    }

    int maxCount = -1;
    ChunkGridCell dominant = { currentChunkX, currentChunkY };
    for (const auto& kv : totalCellCounts) {
        if (kv.second > maxCount) {
            maxCount = kv.second;
            dominant = kv.first;
        }
    }

    return ResolveChunkOwner(dominant, prefix, loadedChunks);
}

std::string ChunkOwnership::DeterminePositionOwner(Vector3 worldPos, 
                                                   const std::string& prefix, 
                                                   const std::vector<ParsedChunk>* loadedChunks) {
    return ResolveChunkOwner(WorldToGridPos(worldPos), prefix, loadedChunks);
}

bool ChunkOwnership::MigrateMesh(LocalGeometryOverlay& overlay, 
                                 int srcObjIdx, 
                                 int srcMeshIdx, 
                                 const std::string& targetChunkName, 
                                 History* history) {
    LoadedChunk* srcLc = nullptr;
    LoadedChunk* dstLc = nullptr;
    if (!ResolveChunkPair(overlay, targetChunkName, srcLc, dstLc)) return false;
    if (srcObjIdx < 0 || srcObjIdx >= static_cast<int>(srcLc->data->objects.size())) return false;

    auto& srcObj = srcLc->data->objects[srcObjIdx];
    if (srcMeshIdx < 0 || srcMeshIdx >= static_cast<int>(srcObj.meshes.size())) return false;

    if (srcObj.meshes.size() == 1) {
        return MigrateObject(overlay, srcObjIdx, targetChunkName, history);
    }

    RenderMesh movedMesh = std::move(srcObj.meshes[srcMeshIdx]);
    srcObj.meshes.erase(srcObj.meshes.begin() + srcMeshIdx);

    RenderObject newDstObj;
    newDstObj.name = srcObj.name + "_M";
    newDstObj.isGlobal = false;
    newDstObj.ipdDataOffset = -1;
    newDstObj.meshes.push_back(std::move(movedMesh));
    RecalculateObjectBounds(newDstObj);

    dstLc->data->objects.push_back(std::move(newDstObj));
    RecalculateObjectBounds(srcObj);

    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(srcLc->data->chunkName, ws);
    overlay.RebuildChunkBatches(dstLc->data->chunkName, ws);

    overlay.m_selectedChunk = targetChunkName;
    overlay.m_selectedObjectIdx = static_cast<int>(dstLc->data->objects.size()) - 1;
    overlay.m_selectedMeshIdx = 0;

    return true;
}

bool ChunkOwnership::MigrateObject(LocalGeometryOverlay& overlay, 
                                   int srcObjIdx, 
                                   const std::string& targetChunkName, 
                                   History* history) {
    LoadedChunk* srcLc = nullptr;
    LoadedChunk* dstLc = nullptr;
    if (!ResolveChunkPair(overlay, targetChunkName, srcLc, dstLc)) return false;
    if (srcObjIdx < 0 || srcObjIdx >= static_cast<int>(srcLc->data->objects.size())) return false;

    RenderObject movedObj = std::move(srcLc->data->objects[srcObjIdx]);
    srcLc->data->objects.erase(srcLc->data->objects.begin() + srcObjIdx);

    if (movedObj.isGlobal) {
        int dxCell = srcLc->data->xPos - dstLc->data->xPos;
        int dyCell = srcLc->data->yPos - dstLc->data->yPos;
        movedObj.rawTx += dxCell * 10240;
        movedObj.rawTz += dyCell * 10240;
    }

    dstLc->data->objects.push_back(std::move(movedObj));

    std::string ws = GetWorkspaceDir(overlay);
    overlay.RebuildChunkBatches(srcLc->data->chunkName, ws);
    overlay.RebuildChunkBatches(dstLc->data->chunkName, ws);

    overlay.m_selectedChunk = targetChunkName;
    overlay.m_selectedObjectIdx = static_cast<int>(dstLc->data->objects.size()) - 1;
    overlay.m_selectedMeshIdx = 0;

    return true;
}

int ChunkOwnership::AutoMigrateMisplacedGeometry(LocalGeometryOverlay& overlay, History* history) {
    int migratedCount = 0;
    std::vector<ParsedChunk> loadedList;
    for (const auto& lc : overlay.GetChunks()) {
        if (lc.data) loadedList.push_back(*lc.data);
    }

    if (loadedList.size() < 2) return 0;

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& lcConst : overlay.GetChunks()) {
            auto& lc = const_cast<LoadedChunk&>(lcConst);
            if (!lc.data) continue;

            for (int oi = static_cast<int>(lc.data->objects.size()) - 1; oi >= 0; --oi) {
                auto& obj = lc.data->objects[oi];
                std::string targetOwner = DetermineObjectOwner(obj, lc.data->xPos, lc.data->yPos, lc.data->chunkPrefix, &loadedList);

                if (!targetOwner.empty() && targetOwner != lc.data->chunkName) {
                    bool isLoaded = false;
                    for (const auto& other : overlay.GetChunks()) {
                        if (other.data && other.data->chunkName == targetOwner) {
                            isLoaded = true;
                            break;
                        }
                    }

                    if (isLoaded) {
                        overlay.m_selectedChunk = lc.data->chunkName;
                        if (MigrateObject(overlay, oi, targetOwner, history)) {
                            migratedCount++;
                            changed = true;
                            break;
                        }
                    }
                }
            }
            if (changed) break;
        }
    }

    return migratedCount;
}

} // namespace Geometry
