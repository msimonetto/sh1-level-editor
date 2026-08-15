#pragma once
#include "core/IPDParse.h"
#include <string>
#include <vector>
#include <cstdint>

#include <array>

// ---------------------------------------------------------------------------
// IPDWrite — Section-aware binary patch writer for IPD and PLM files.
//
// Write strategy: "Intelligent Section Patcher"
//   - The source file is read into a mutable std::vector<uint8_t> (working buf).
//   - Same-size edits (UV/CBA/texnum changes): directly overwrite bytes at the
//     cached packRawOffset. No offset relocation needed.
//   - Size-changing edits (future: add/remove vertex, pack, mesh): the affected
//     section is replaced in the buffer, then RelocateIPDOffsets / RelocatePLMOffsets
//     adjust every absolute offset pointer in the file header(s) that falls
//     past the insertion point.
//   - The final buffer is written atomically (temp file + rename).
//
// Save target:
//   ipdPath  = workspace/chunks/{CHUNK}.IPD   (always written)
//   glbPath  = workspace/geometry/{PREFIX}_GLB.PLM  (written if global faces exist)
// ---------------------------------------------------------------------------
class IPDWrite {
public:
    // -----------------------------------------------------------------------
    // WriteChunk: apply all in-memory face modifications from 'chunk' to disk.
    // Returns false on I/O error (logs to stdout).
    // If outPatchedCount != nullptr, receives the number of pack headers written.
    // If outFilesWritten != nullptr, receives true if any file was modified on disk.
    //
    // NOTE: chunk is taken by non-const reference so that ipdDataOffset can be
    // written back into newly-inserted RenderObjects after insertion, and so that
    // all existing ipdDataOffset values can be updated when bytes are shifted by
    // RelocateIPDOffsets.  This prevents re-insertion on subsequent WriteChunk calls.
    // -----------------------------------------------------------------------
    static bool WriteChunk(const std::string& ipdPath,
                           const std::string& glbPath,
                           ParsedChunk& chunk,
                           int* outPatchedCount = nullptr,
                           bool* outFilesWritten = nullptr);

    // -----------------------------------------------------------------------
    // Validate: check structural integrity before writing.
    // Returns a list of warning strings (empty = clean).
    // Does NOT block the save — caller decides whether to proceed.
    // Currently always returns empty (stub for future checks).
    // -----------------------------------------------------------------------
    static std::vector<std::string> Validate(const ParsedChunk& chunk);

private:
    // -----------------------------------------------------------------------
    // Structural Update Pass: Dynamically resize PLM_DATA arrays if vertex or face
    // counts changed. Then encode all vertices and faces (including new ones) back
    // into the shifted buffer. Relocates all subsequent offsets automatically.
    // Returns the number of faces patched/encoded.
    // -----------------------------------------------------------------------
    static int UpdateMeshStructure(std::vector<uint8_t>& buf,
                                   const ParsedChunk& chunk,
                                   bool isGlobalFile);

    // Encode one RenderFace's UV/CBA/texnum AND vertex indices back into the
    // working buffer at the specified absolute byte offset `pkOff`.
    static void EncodeFaceAtOffset(std::vector<uint8_t>& buf, const RenderFace& face, int pkOff);

    // Reverse the UV bias and re-encode normalised float UVs to uint8.
    // bias: the original rawU/rawV arrays are used to detect which vertex had
    // bias applied (+1). The bias is subtracted before rounding, then clamped
    // to [0, 255]. This preserves uint8 precision matching the game's data type.
    static void EncodeUVs(const RenderFace& face,
                          uint8_t outU[4], uint8_t outV[4]);

    // -----------------------------------------------------------------------
    // Section-relocation path (stubs — future: vertex/face addition support)
    // -----------------------------------------------------------------------

    // Adjust all absolute offset fields in an IPD file working buffer after
    // 'delta' bytes are inserted at (or removed from) 'insertionPoint'.
    // delta > 0: insertion. delta < 0: removal.
    // Adjusts: IPD_FILE_HEADER (plm_offset, obj_name_offset, obj_data_offset,
    //          unkdata_offset), PLM_FILE_HEADER (tex_name_offset,
    //          obj_start_offset, unk_data_offset), PLM_OBJ_HEADER.data_offset
    //          (all objects), PLM_DATA_HEADER offset fields (all meshes).
    static void RelocateIPDOffsets(std::vector<uint8_t>& buf,
                                   int insertionPoint,
                                   int delta);

    // Same as above for a standalone PLM file (e.g. _GLB.PLM).
    // Adjusts: PLM_FILE_HEADER offsets, PLM_OBJ_HEADER.data_offset,
    //          PLM_DATA_HEADER offset fields.
    static void RelocatePLMOffsets(std::vector<uint8_t>& buf,
                                   int insertionPoint,
                                   int delta,
                                   int plmBase = 0);

    // Re-derive the byte offset of a PLM_PACK_HEADER by navigating the live
    // working buffer using FaceAddress logical indices.
    // Used to refresh stale packRawOffset caches after a relocation, and as a
    // consistency check (debug builds).
    // Returns -1 on failure (object not found, index out of range).
    static int ResolveFaceOffset(const std::vector<uint8_t>& buf,
                                 const FaceAddress& addr,
                                 int plmBase);

    // -----------------------------------------------------------------------
    // Utility
    // -----------------------------------------------------------------------

    // Read entire file into a vector. Returns false on failure.
    static bool ReadFile(const std::string& path, std::vector<uint8_t>& out);

    // Compute 256-bit SHA256 checksum of a buffer
    static std::array<uint32_t, 8> CalculateSHA256(const std::vector<uint8_t>& buf);

    // Atomically write 'buf' to 'path' (write to temp, rename).
    // Returns false on failure.
    static bool WriteFileAtomic(const std::string& path,
                                const std::vector<uint8_t>& buf);

    // Little-endian helpers
    static inline int32_t  ReadI32(const std::vector<uint8_t>& buf, int off) {
        int32_t v; memcpy(&v, buf.data() + off, 4); return v;
    }
    static inline void WriteI32(std::vector<uint8_t>& buf, int off, int32_t v) {
        memcpy(buf.data() + off, &v, 4);
    }
    static inline void WriteU16(std::vector<uint8_t>& buf, int off, uint16_t v) {
        memcpy(buf.data() + off, &v, 2);
    }
};
