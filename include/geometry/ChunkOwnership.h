#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "raylib.h"
#include "formats/IPDParse.h"
#include "core/ChunkUtils.h"

class LocalGeometryOverlay;
class History;
struct LoadedChunk;

namespace Geometry {

// ---------------------------------------------------------------------------
// ChunkGridCell — 2D grid cell coordinate representing a 40x40m Silent Hill 1 chunk.
// ---------------------------------------------------------------------------
struct ChunkGridCell {
    int8_t x = 0;
    int8_t y = 0;

    bool operator==(const ChunkGridCell& o) const;
    bool operator!=(const ChunkGridCell& o) const;
    bool operator<(const ChunkGridCell& o) const;
};

// ---------------------------------------------------------------------------
// ChunkOccupancy — Spatial coverage analysis of a mesh or object across chunks.
// ---------------------------------------------------------------------------
struct ChunkOccupancy {
    std::vector<ChunkGridCell> occupiedCells;
    ChunkGridCell              dominantCell = {0, 0};
    bool                       spansMultipleChunks = false;
    std::string                suggestedChunkName;

    bool IsOwnedBy(int8_t x, int8_t y) const;
};

// ---------------------------------------------------------------------------
// ChunkOwnership — Spatial ownership, coordinate mapping, and chunk migration engine.
// ---------------------------------------------------------------------------
class ChunkOwnership {
public:
    static constexpr float GRID_SIZE = 40.0f;           // 10240 raw units = 40.0 world units
    static constexpr float OVERHANG_WORLD = 16.0f / 256.0f; // 16 raw units (~0.0625 world units)

    // Calculate which chunk grid cell (x, y) contains a world position
    static ChunkGridCell WorldToGridPos(Vector3 worldPos);

    // Get 3D AABB bounding box for a chunk grid cell in world space
    static BoundingBox GridToWorldBounds(int8_t xPos, int8_t yPos, bool includeOverhang = true);

    // Get 3D world space center position for a chunk grid cell (at Y=0)
    static Vector3 GridToWorldCenter(int8_t xPos, int8_t yPos);

    // Naming & Parsing (Standardized in Core::ChunkUtils)
    static std::string FormatChunkName(const std::string& prefix, int8_t gx, int8_t gy);
    static bool ParseChunkName(const std::string& chunkName, std::string& outPrefix, int8_t& outX, int8_t& outY);
    static std::string ExtractChunkPrefix(const std::string& chunkName);

    // Analyze spatial cell coverage of all vertices in a RenderMesh
    static ChunkOccupancy AnalyzeMeshOccupancy(const RenderMesh& mesh, 
                                              int8_t currentChunkX, 
                                              int8_t currentChunkY, 
                                              const std::string& prefix = "");

    // Determine the ideal owner chunk name for a mesh based on vertex distribution
    static std::string DetermineMeshOwner(const RenderMesh& mesh, 
                                         int8_t currentChunkX, 
                                         int8_t currentChunkY, 
                                         const std::string& prefix, 
                                         const std::vector<ParsedChunk>* loadedChunks = nullptr);

    // Determine the ideal owner chunk name for a RenderObject (local or global prop)
    static std::string DetermineObjectOwner(const RenderObject& obj, 
                                           int8_t currentChunkX, 
                                           int8_t currentChunkY, 
                                           const std::string& prefix, 
                                           const std::vector<ParsedChunk>* loadedChunks = nullptr);

    // Determine the ideal owner chunk name for any arbitrary world position
    static std::string DeterminePositionOwner(Vector3 worldPos, 
                                             const std::string& prefix, 
                                             const std::vector<ParsedChunk>* loadedChunks = nullptr);

    // Migrate a single mesh from its parent object to a target chunk
    static bool MigrateMesh(LocalGeometryOverlay& overlay, 
                            int srcObjIdx, 
                            int srcMeshIdx, 
                            const std::string& targetChunkName, 
                            History* history = nullptr);

    // Migrate an entire object (local or global prop) to a target chunk
    static bool MigrateObject(LocalGeometryOverlay& overlay, 
                              int srcObjIdx, 
                              const std::string& targetChunkName, 
                              History* history = nullptr);

    // Automatically scan loaded chunks and migrate all misplaced meshes/objects to their respective loaded chunk owners
    static int AutoMigrateMisplacedGeometry(LocalGeometryOverlay& overlay, 
                                            History* history = nullptr);
};

} // namespace Geometry
