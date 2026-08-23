#pragma once
#include <vector>
#include <cstdint>
#include <functional>
#include "formats/Structs.h"
#include "formats/IPDParse.h" // For RenderFace, RenderObject etc

class PLMWrite {
public:
    // Encode one RenderFace's UV/CBA/texnum AND vertex indices back into the buffer
    static void EncodeFaceAtOffset(std::vector<uint8_t>& buf, const RenderFace& face, int pkOff);

    // Reverse the UV bias and re-encode normalised float UVs to uint8
    static void EncodeUVs(const RenderFace& face, uint8_t outU[4], uint8_t outV[4]);

    // RelocatePLMOffsets: Adjust absolute offset fields in a PLM file working buffer
    static void RelocatePLMOffsets(std::vector<uint8_t>& buf, int insertionPoint, int delta, int plmBase = 0);

    // UpdateMeshStructure: Dynamically resize PLM_DATA arrays
    // relocateFn is called whenever bytes are inserted/deleted so the caller can relocate its own offsets
    static int UpdateMeshStructure(std::vector<uint8_t>& buf,
                                   int plmBase,
                                   const ParsedChunk& chunk,
                                   bool isGlobalFile,
                                   std::function<void(std::vector<uint8_t>&, int, int)> relocateFn);

    // Re-derive the byte offset of a PLM_PACK_HEADER
    static int ResolveFaceOffset(const std::vector<uint8_t>& buf, const FaceAddress& addr, int plmBase);
};
