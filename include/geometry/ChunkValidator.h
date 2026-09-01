#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include "formats/IPDParse.h"
#include "geometry/ChunkOwnership.h"
#include "raylib.h"

struct LoadedChunk;

// ---------------------------------------------------------------------------
// IPD Chunk Validation Constants & Constraints
// ---------------------------------------------------------------------------
// Scaling: 1 world unit = 256 raw IPD units (1/256.0f)
// Grid cell: 40.0f x 40.0f world units (10240 raw units = IPD_MAP_MAX)
// Height limit: +/- 16.0f world units = (16 << 8) = 4096 raw units
// Overhang constraint: 16 raw units = (16.0f / 256.0f) world units (0.0625f)
// Vertex budget: 255 vertices max per mesh (PS1 PLM uint8 vertex index limit)
// ---------------------------------------------------------------------------
static constexpr float VALIDATOR_SCALE          = 1.0f / 256.0f;
static constexpr float VALIDATOR_GRID_SIZE      = 40.0f;              // 10240 raw units
static constexpr float VALIDATOR_MAX_HEIGHT     = 16.0f;              // 16 << 8 = 4096 raw units
static constexpr float VALIDATOR_MIN_HEIGHT     = -16.0f;             // -(16 << 8) = -4096 raw units
static constexpr int   VALIDATOR_OVERHANG_RAW   = 16;                 // 16 raw IPD units
static constexpr float VALIDATOR_OVERHANG_WORLD = 16.0f / 256.0f;    // ~0.0625f
static constexpr int   VALIDATOR_MAX_VERTS      = 255;
static constexpr int   VALIDATOR_WARN_VERTS     = 240;

enum class ValidationSeverity {
    Warning,
    Error
};

enum class ValidationIssueType {
    HeightExceeded,
    HeightBelowMin,
    OverhangExceeded,
    VertexCapacityExceeded,
    VertexCapacityNearLimit,
    WrongChunkOwner,
    CrossChunkMesh,
    GlobalObjectMisplaced,
    InvalidVertexIndex,
    DegenerateFace,
    TextureIndexOutOfRange
};

struct ValidationIssue {
    ValidationSeverity  severity  = ValidationSeverity::Warning;
    ValidationIssueType type      = ValidationIssueType::HeightExceeded;
    std::string         message;
    std::string         chunkName;
    int                 objectIdx = -1;
    int                 meshIdx   = -1;
    int                 vertexIdx = -1;
    int                 faceIdx   = -1;
    Vector3             worldPos  = {0, 0, 0};
};

struct ValidationResult {
    int errorCount   = 0;
    int warningCount = 0;
    std::vector<ValidationIssue> issues;

    bool HasErrors() const;
    bool HasWarnings() const;
    bool IsClean() const;
};

class ChunkValidator {
public:
    // Validate an entire ParsedChunk against all hardware, topology, and spatial constraints
    static ValidationResult ValidateChunk(const ParsedChunk& chunk, 
                                          const std::vector<ParsedChunk>* allLoadedChunks = nullptr);

    // Validate a single RenderObject (all its meshes + global prop anchor positioning)
    static ValidationResult ValidateObject(const RenderObject& obj, 
                                           const ParsedChunk& chunk, 
                                           int objectIdx, 
                                           const std::vector<ParsedChunk>* allLoadedChunks = nullptr);

    // Validate a single RenderMesh (vertex count, bounds, face topology, and texture index ranges)
    static ValidationResult ValidateMesh(const RenderMesh& mesh, 
                                         const RenderObject& obj, 
                                         const ParsedChunk& chunk, 
                                         int objectIdx, 
                                         int meshIdx);

    // Validate a single world vertex position against chunk cell bounds & height envelope
    static bool ValidateVertexPosition(Vector3 worldPos, 
                                       int8_t xPos, 
                                       int8_t yPos, 
                                       ValidationIssue* outIssue = nullptr);

    // Validate all loaded chunks in the active scene
    static ValidationResult ValidateLoadedChunks(const std::vector<LoadedChunk>& chunks);

    // --- Spatial Ownership Convenience Forwarders (Delegates to ChunkOwnership) ---
    static Geometry::ChunkGridCell DetermineChunkGridPos(Vector3 worldPos);

    static std::string DetermineChunkOwnerForMesh(const RenderMesh& mesh, 
                                                  const RenderObject& obj, 
                                                  const std::vector<ParsedChunk>& allChunks);
};
