#pragma once
class Viewport;
#include "formats/IPDParse.h"
#include "viewport/ViewportOverlay.h"
#include <vector>

struct SubcellLine {
    Vector3 a;
    Vector3 b;
    Color c;
};

struct CollisionBatch {
    Mesh mesh = {0};
    Material material = {0};
    bool meshUploaded = false;
    
    CollisionBatch() = default;
    CollisionBatch(const CollisionBatch&) = delete;
    CollisionBatch& operator=(const CollisionBatch&) = delete;
    CollisionBatch(CollisionBatch&& other) noexcept;
    CollisionBatch& operator=(CollisionBatch&& other) noexcept;
    ~CollisionBatch();
};

struct CollisionChunkData {
    std::string chunkName;
    bool visible = true;
    int8_t xPos = 0;
    int8_t yPos = 0;
    
    std::vector<CollisionBatch> terrainBatches; // Typically one for walkable, one for non-walkable
    std::vector<CollisionBatch> wallBatches;
    std::vector<CollisionBatch> visualBatches;  // Visual geometry faces + wireframes
    std::vector<Vector3> splitVertices;
    std::vector<SubcellLine> floorLines;
    
    BoundingBox bounds;
    
    CollisionChunkData() = default;
    CollisionChunkData(const CollisionChunkData&) = delete;
    CollisionChunkData& operator=(const CollisionChunkData&) = delete;
    CollisionChunkData(CollisionChunkData&&) noexcept = default;
    CollisionChunkData& operator=(CollisionChunkData&&) noexcept = default;
};

// ---------------------------------------------------------------------------
// CollisionOverlay — Dedicated overlay for collision geometry (Terrain + Walls)
// ---------------------------------------------------------------------------
class CollisionOverlay : public ViewportOverlay {
public:
    CollisionOverlay() = default;
    ~CollisionOverlay();

    // Load an IPD file's collision data into the scene
    bool LoadChunk(const ParsedChunk& parsedChunk);

    // Remove a loaded chunk by name
    void UnloadChunk(const std::string& chunkName);
    
    // Clear all chunks
    void UnloadAll() override;

    const std::vector<CollisionChunkData>& GetChunks() const { return m_chunks; }
    std::vector<CollisionChunkData>&       GetChunks()       { return m_chunks; }

    bool m_showVisualGeometry = true;

    void DrawOverlay(Viewport& vp) override;
    void HandlePicking(Viewport& vp, Ray ray) override;

private:
    std::vector<CollisionChunkData> m_chunks;

    void BuildCollisionBatches(CollisionChunkData& outData, const ParsedCollision& collision);
    void FreeCollisionBatches(CollisionChunkData& chunkData);
    
    // Helper to convert Silent Hill world coordinates to Raylib OpenGL coordinates
    // (X/Z reversed from IPD coordinates, scaled by 1/4096.0f)
    Vector3 WorldFromRaw(int32_t rawX, int32_t rawY, int32_t rawZ, const ParsedCollision& coll);
};
