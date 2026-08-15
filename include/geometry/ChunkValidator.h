#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include "core/IPDParse.h"
#include "raylib.h"

// ---------------------------------------------------------------------------
// IPD Chunk Validation Constants & Constraints
// ---------------------------------------------------------------------------
// Scaling: 1 world unit = 256 raw IPD units (1/256.0f)
// Grid cell: 40.0f x 40.0f world units (10240 raw units = IPD_MAP_MAX)
// Height limit: 16.0f world units = (16 << 8) = 4096 raw units
// Overhang constraint: 7 raw units = (7.0f / 256.0f) world units (~0.02734f)
// Vertex budget: 255 vertices max per mesh (PS1 PLM uint8 vertex index limit)
// ---------------------------------------------------------------------------
static constexpr float VALIDATOR_SCALE          = 1.0f / 256.0f;
static constexpr float VALIDATOR_GRID_SIZE      = 40.0f;              // 10240 raw units
static constexpr float VALIDATOR_MAX_HEIGHT     = 16.0f;              // 16 << 8 = 4096 raw units
static constexpr float VALIDATOR_MIN_HEIGHT     = 0.0f;
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
    VertexCapacityNearLimit
};

struct ValidationIssue {
    ValidationSeverity  severity;
    ValidationIssueType type;
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

    bool HasErrors() const { return errorCount > 0; }
    bool HasWarnings() const { return warningCount > 0; }
    bool IsClean() const { return errorCount == 0 && warningCount == 0; }
};

class ChunkValidator {
public:
    // Validate an entire ParsedChunk against all constraints
    static ValidationResult ValidateChunk(const ParsedChunk& chunk);

    // Validate a single RenderMesh within a RenderObject and ParsedChunk
    static ValidationResult ValidateMesh(const RenderMesh& mesh, 
                                          const RenderObject& obj, 
                                          const ParsedChunk& chunk, 
                                          int objectIdx, 
                                          int meshIdx);

    // Validate a single world vertex position against chunk cell bounds & height
    static bool ValidateVertexPosition(Vector3 worldPos, 
                                        int8_t xPos, 
                                        int8_t yPos, 
                                        ValidationIssue* outIssue = nullptr);

    // Calculate which chunk grid cell (xPos, yPos) contains a world position
    static std::pair<int8_t, int8_t> DetermineChunkGridPos(Vector3 worldPos);

    // Determine which chunk owner contains a mesh
    static std::string DetermineChunkOwnerForMesh(const RenderMesh& mesh, 
                                                   const RenderObject& obj, 
                                                   const std::vector<ParsedChunk>& allChunks);
};
