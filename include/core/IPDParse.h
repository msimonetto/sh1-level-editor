#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "raylib.h"

#include <cctype>

// ---------------------------------------------------------------------------
// Coordinate constants (faithful port of coordinate_math.py)
// ---------------------------------------------------------------------------
static constexpr float IPD_SCALE   = 1.0f / 256.0f;  // SCALE = 1/256
static constexpr float IPD_MAP_MAX = 10240.0f;         // MAP_MAX

// ---------------------------------------------------------------------------
// Helper for chunk prefix extraction from chunk name / filename.
// Standard IPD naming convention: <PREFIX><X_HEX><Z_HEX> (e.g. THR0000 -> THR, THRFBFE -> THR, SC0102 -> SC)
// ---------------------------------------------------------------------------
inline std::string DeriveChunkPrefix(const std::string& name) {
    if (name.length() > 4) {
        std::string tail = name.substr(name.length() - 4);
        bool isHex4 = true;
        for (char c : tail) {
            if (!isxdigit((unsigned char)c)) {
                isHex4 = false;
                break;
            }
        }
        if (isHex4) {
            return name.substr(0, name.length() - 4);
        }
    }
    return name;
}

// ---------------------------------------------------------------------------
// FaceAddress — canonical, location-stable address of a face in the PLM binary.
// Used by IPDWrite for patch-in-place write-back and future section relocation.
// ---------------------------------------------------------------------------
struct FaceAddress {
    std::string plmObjectName; // 8-char PLM object name (key into PLM_OBJ_HEADER)
    int         meshIdx  = 0;  // index into PLM_DATA_HEADER array for this object
    int         packIdx  = 0;  // index into PLM_PACK_HEADER array for this mesh
    bool        isGlobal = false; // false = local PLM inside .IPD, true = _GLB.PLM
    int32_t     packRawOffset = -1; // cached absolute byte offset of PLM_PACK_HEADER
                                    // within its source file (IPD or GLB).
                                    // Valid at parse time; invalidated after any
                                    // size-changing operation on the same file.
};

// ---------------------------------------------------------------------------
// Per-face render data
//   uv[4][2]    — pre-computed normalised UVs with +1-bias applied
//   v[4]        — local vertex indices; v[3] == 0xFF means triangle
//   paletteRow  — CLUT row index decoded from cba: (cba & 0x7FC0) >> 6
//   texNum      — local texture index from PLM; 0x7F means no-texture
//   cbaRaw      — raw CBA word, stored for future UV-editor use
//   addr        — canonical write-back address (see FaceAddress)
//   rawU/rawV   — original uint8 UV bytes from binary BEFORE bias/normalisation
//                 preserved for precision round-trip on save
// ---------------------------------------------------------------------------
struct RenderFace {
    uint8_t     v[4];
    float       uv[4][2];
    uint8_t     texNum;      // Raw index (0x7F = none)
    std::string texName;     // Resolved name (e.g. "THRFF01_TEX")
    uint8_t     paletteRow;
    uint16_t    cbaRaw;
    FaceAddress addr;        // Logical + cached physical address for write-back
    uint8_t     rawU[4];    // Original PLM_PACK_HEADER u0..u3 before bias
    uint8_t     rawV[4];    // Original PLM_PACK_HEADER v0..v3 before bias
    uint8_t     unk1;       // Preserved unk1 byte (lighting/color flags)
    uint8_t     origTexByte;// Preserved tex_num_and_unk2_byte (bit 7 flags)
    uint8_t     normals[4]; // Preserved normal indices for lighting
};

// ---------------------------------------------------------------------------
// One PLM_DATA_HEADER worth of geometry
// ---------------------------------------------------------------------------
struct RenderMesh {
    std::vector<float>      vx, vy, vz; // local vertices (already scaled)
    std::vector<RenderFace> faces;
};

// ---------------------------------------------------------------------------
// One PLM_OBJ_HEADER placed at a world position.
// World transform is baked into vx/vy/vz at parse time for Phase 5 simplicity.
// The raw matrix components are preserved here for future editing use.
// ---------------------------------------------------------------------------
struct RenderObject {
    std::string name;
    bool        isGlobal;  // true = came from _GLB.PLM

    // Raw transform (for future editing / round-trip back to IPD)
    int32_t rawTx, rawTy, rawTz;
    int16_t rt[3][3]; // rotation matrix [row][col], 4096-fixed-point

    // Byte offset into the .IPD file where this object's IPD_OBJ_DATA lives
    int ipdDataOffset = -1;
    
    // Group and dictionary tracking for creating new placements
    int ipdObjId = -1;
    int ipdPosGroup = -1;

    BoundingBox bounds;

    std::vector<RenderMesh> meshes;
};

// ---------------------------------------------------------------------------
// Batch: all faces that share the same (texNum, paletteRow) within one chunk.
// Built after parse time; contains pre-uploaded GPU handles filled by Viewport.
// ---------------------------------------------------------------------------
struct RenderBatch {
    std::string texName; // Resolved texture name (empty if none)
    uint8_t paletteRow;

    // Interleaved vertex data for Raylib Mesh upload: position + UV
    std::vector<float> positions; // 3 floats per vertex
    std::vector<float> texcoords; // 2 floats per vertex
    int vertexCount = 0;
};

// ---------------------------------------------------------------------------
// Collision Data Extracted from IPD_COLL_HEADER
// ---------------------------------------------------------------------------
struct ParsedCollision {
    int32_t positionX = 0;
    int32_t positionZ = 0;
    uint16_t gridScale = 512;
    uint8_t gridWidth = 20;
    uint8_t gridHeight = 20;

    struct SplitVertex { int16_t x, y, z; };
    struct Surface { int16_t baseGroundHeight; uint16_t tilt_flags; };
    struct Subcell { uint8_t splitVertexIdx0, splitVertexIdx1, surfaceIdx0, surfaceIdx1; };
    struct GridCell { int16_t start, end; }; // Start/end indices into block5

    std::vector<SplitVertex> splitVertices;
    std::vector<Surface> surfaces;
    std::vector<Subcell> subcells;
    std::vector<GridCell> grid; // Array of gridWidth * gridHeight
    std::vector<uint8_t> block5; // Indirection table for subcells
    std::vector<uint8_t> block6; // Indirection table for subcells

    bool hasCollision = false;
};

// ---------------------------------------------------------------------------
// One parsed chunk — all geometry and metadata needed for rendering
// ---------------------------------------------------------------------------
struct ParsedChunk {
    std::string chunkName;       // e.g. "THR0000"
    std::string chunkPrefix;     // e.g. "THR"
    int8_t      xPos = 0;
    int8_t      yPos = 0;

    ParsedCollision collision;


    // Texture names from the local PLM (index == texNum in face data)
    std::vector<std::string> localTexNames;
    // Texture names from the global PLM (index == texNum in global-object face data)
    std::vector<std::string> globalTexNames;

    // All placed objects (local + global, world coords baked into vertices)
    std::vector<RenderObject> objects;

    // Flat draw batches built from objects (filled by BuildBatches())
    std::vector<RenderBatch> batches;

    bool loaded = false;
};

// ---------------------------------------------------------------------------
// IPDParse — loads an IPD file (and its associated GLB PLM) into a ParsedChunk
// ---------------------------------------------------------------------------
class IPDParse {
public:
    // Parse an IPD file at `ipdPath`.  `assetsDir` is the workspace root used
    // to resolve sibling TIM files and the _GLB.PLM.  Returns false on failure.
    static bool Parse(const std::string& ipdPath,
                      const std::string& workspaceDir,
                      ParsedChunk&       out);

    // After Parse(), call BuildBatches() to flatten object geometry into per-
    // (texNum, paletteRow) draw batches.  Viewport calls this before GPU upload.
    static void BuildBatches(ParsedChunk& chunk);

    // Parse a standalone _GLB.PLM binary (without an IPD).  Each PLM_OBJ_HEADER
    // is extracted with an identity world transform (object-local space).
    // On success fills outObjects (one RenderObject per PLM entry), outTexNames
    // (per-file texture name table), and outObjPackCounts.
    // Returns false on any read/format error.
    struct GlbObjectInfo {
        std::string name;    // 8-char PLM name, null-stripped
        int mesh_id;         // position index in PLM obj_headers array
        int pack_count;      // total pack count across all submeshes
    };
    static bool ParseGlbFile(const std::string&             glbPath,
                              std::vector<RenderObject>&     outObjects,
                              std::vector<std::string>&      outTexNames,
                              std::vector<GlbObjectInfo>&    outInfo);

private:
    // Parse one PLM section (embedded or standalone file).
    // plm_base = absolute byte offset inside `buf` where PLM header starts.
    // Appends to `out.objects`; uses texNames for face.texNum lookup.
    static bool ParsePLMSection(const std::vector<uint8_t>& buf,
                                 int                         plmBase,
                                 const std::vector<std::string>& texNames,
                                 bool                        isGlobal,
                                 int8_t                      xPos,
                                 int8_t                      yPos,
                                 float                       worldTx,
                                 float                       worldTy,
                                 float                       worldTz,
                                 float                       rot[3][3],
                                 int32_t                     rawTx,
                                 int32_t                     rawTy,
                                 int32_t                     rawTz,
                                 int16_t                     rawRot[3][3],
                                 RenderObject&               outObj);
};
