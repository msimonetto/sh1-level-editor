#include "formats/IPDWrite.h"
#include "formats/Structs.h"
#include "formats/PLMWrite.h"
#include "formats/PLMParse.h"
#include "raylib.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Utility: Read file
// ---------------------------------------------------------------------------
bool IPDWrite::ReadFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        printf("[IPDWrite] Cannot open for reading: %s\n", path.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return false; }
    out.resize((size_t)sz);
    bool ok = (fread(out.data(), 1, sz, f) == (size_t)sz);
    fclose(f);
    if (!ok) printf("[IPDWrite] Read error: %s\n", path.c_str());
    return ok;
}

// ---------------------------------------------------------------------------
// Utility: Compute SHA256
// ---------------------------------------------------------------------------
std::array<uint32_t, 8> IPDWrite::CalculateSHA256(const std::vector<uint8_t>& buf) {
    std::array<uint32_t, 8> hash{};
    if (buf.empty()) return hash;
    unsigned int* ptr = ComputeSHA256(buf.data(), static_cast<int>(buf.size()));
    if (ptr) {
        for (int i = 0; i < 8; ++i) {
            hash[i] = ptr[i];
        }
    }
    return hash;
}

// ---------------------------------------------------------------------------
// Utility: Atomic write (temp file + rename)
// ---------------------------------------------------------------------------
bool IPDWrite::WriteFileAtomic(const std::string& path, const std::vector<uint8_t>& buf) {
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) {
        printf("[IPDWrite] Cannot open temp for writing: %s\n", tmp.c_str());
        return false;
    }
    bool ok = (fwrite(buf.data(), 1, buf.size(), f) == buf.size());
    fclose(f);
    if (!ok) {
        printf("[IPDWrite] Write error: %s\n", tmp.c_str());
        fs::remove(tmp);
        return false;
    }
    // Atomic rename
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        printf("[IPDWrite] Rename failed %s -> %s: %s\n",
               tmp.c_str(), path.c_str(), ec.message().c_str());
        fs::remove(tmp);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// WriteChunk: orchestrate the full write operation
// ---------------------------------------------------------------------------
bool IPDWrite::WriteChunk(const std::string& ipdPath,
                           const std::string& glbPath,
                           ParsedChunk& chunk,
                           int* outPatchedCount,
                           bool* outFilesWritten) {
    int totalPatched = 0;
    bool anyWritten = false;

    // --- Patch local (IPD-embedded PLM) ---
    {
        std::vector<uint8_t> ipdBuf;
        if (!ReadFile(ipdPath, ipdBuf)) return false;

        std::array<uint32_t, 8> origHash = CalculateSHA256(ipdBuf);

        // Insert new IPD_OBJ_DATA entries for objects that were created in memory
        // but don't yet have a binary record (ipdDataOffset == -1).
        //
        // After each insertion we must:
        //   a) Record the insertion offset back onto the RenderObject so that
        //      subsequent WriteChunk calls don't re-insert the same entry.
        //   b) Shift every other object's ipdDataOffset that lies at or after
        //      the insertion point, so patch-in-place writes later in this
        //      function still land at the correct location.
        IPD_FILE_HEADER* ipdHdr = (IPD_FILE_HEADER*)ipdBuf.data();
        if (ipdHdr->obj_data_offset > 0) {
            for (auto& ro : chunk.objects) {
                if (ro.ipdDataOffset != -1) continue;          // already placed
                if (!ro.isGlobal)            continue;          // local objects have no IPD_OBJ_DATA row
                if (ro.ipdPosGroup < 0 || ro.ipdObjId < 0) continue;
                if (ro.ipdPosGroup >= ipdHdr->pos_num)     continue;

                int posOff = ipdHdr->obj_data_offset + ro.ipdPosGroup * (int)sizeof(IPD_POS_HEADER);
                IPD_POS_HEADER* pos = (IPD_POS_HEADER*)(ipdBuf.data() + posOff);

                // Insert immediately after the last existing entry in this group,
                // but BEFORE the 8-byte unk2 block (which lives at unk2_offset).
                // The correct insert point is data_offset + current_obj_num * 36.
                int insertOff = pos->data_offset + pos->obj_num * (int)sizeof(IPD_OBJ_DATA);

                IPD_OBJ_DATA newDta;
                memset(&newDta, 0, sizeof(newDta));
                newDta.obj_id = ro.ipdObjId;
                newDta.tx = ro.rawTx;
                newDta.ty = ro.rawTy;
                newDta.tz = ro.rawTz;
                // Identity rotation in PS1 fixed-point (4096 = 1.0): diagonal only.
                // Copy from source object — it was copy-constructed so rt is valid.
                newDta.rt11 = ro.rt[0][0]; newDta.rt12 = ro.rt[0][1]; newDta.rt13 = ro.rt[0][2];
                newDta.rt21 = ro.rt[1][0]; newDta.rt22 = ro.rt[1][1]; newDta.rt23 = ro.rt[1][2];
                newDta.rt31 = ro.rt[2][0]; newDta.rt32 = ro.rt[2][1]; newDta.rt33 = ro.rt[2][2];

                ipdBuf.insert(ipdBuf.begin() + insertOff,
                              (uint8_t*)&newDta,
                              (uint8_t*)&newDta + sizeof(IPD_OBJ_DATA));

                // Re-fetch pointers — ipdBuf may have reallocated
                ipdHdr = (IPD_FILE_HEADER*)ipdBuf.data();
                pos = (IPD_POS_HEADER*)(ipdBuf.data() + ipdHdr->obj_data_offset
                                        + ro.ipdPosGroup * (int)sizeof(IPD_POS_HEADER));
                pos->obj_num++;

                // Adjust all binary offsets that fall at or after the insertion point
                RelocateIPDOffsets(ipdBuf, insertOff, (int)sizeof(IPD_OBJ_DATA));

                // --- Bug A+B fix: record the final offset back into memory ---
                // Note: insertOff itself is now at the right position in the updated
                // binary.  All offsets >= insertOff were bumped by RelocateIPDOffsets,
                // so we also propagate that shift to every other RenderObject that had
                // already been placed (ipdDataOffset > 0 and >= insertOff).
                ro.ipdDataOffset = insertOff;
                for (auto& other : chunk.objects) {
                    if (&other == &ro) continue;
                    if (other.ipdDataOffset > 0 && other.ipdDataOffset >= insertOff) {
                        other.ipdDataOffset += (int)sizeof(IPD_OBJ_DATA);
                    }
                }
            }
        }

        // Patch IPD_OBJ_DATA (object placements and rotations)
        for (const auto& ro : chunk.objects) {
            if (ro.ipdDataOffset > 0 && ro.ipdDataOffset + (int)sizeof(IPD_OBJ_DATA) <= (int)ipdBuf.size()) {
                IPD_OBJ_DATA* dta = (IPD_OBJ_DATA*)(ipdBuf.data() + ro.ipdDataOffset);
                dta->tx = ro.rawTx;
                dta->ty = ro.rawTy;
                dta->tz = ro.rawTz;
                
                dta->rt11 = ro.rt[0][0];
                dta->rt12 = ro.rt[0][1];
                dta->rt13 = ro.rt[0][2];
                dta->rt21 = ro.rt[1][0];
                dta->rt22 = ro.rt[1][1];
                dta->rt23 = ro.rt[1][2];
                dta->rt31 = ro.rt[2][0];
                dta->rt32 = ro.rt[2][1];
                dta->rt33 = ro.rt[2][2];
            }
        }

        int n = PLMWrite::UpdateMeshStructure(ipdBuf, ipdHdr->plm_offset, chunk, false, [](std::vector<uint8_t>& b, int p, int d) { IPDWrite::RelocateIPDOffsets(b, p, d); });
        std::array<uint32_t, 8> newHash = CalculateSHA256(ipdBuf);

        if (origHash != newHash) {
            if (!WriteFileAtomic(ipdPath, ipdBuf)) return false;
            totalPatched += n;
            anyWritten = true;
            printf("[IPDWrite] Patched %d local face(s) in %s\n", n, ipdPath.c_str());
        }
    }

    // --- Patch global (_GLB.PLM) faces if any ---
    bool hasGlobal = false;
    for (const auto& obj : chunk.objects) {
        for (const auto& mesh : obj.meshes) {
            for (const auto& face : mesh.faces) {
                if (face.addr.isGlobal) { hasGlobal = true; break; }
            }
            if (hasGlobal) break;
        }
        if (hasGlobal) break;
    }

    if (hasGlobal) {
        if (glbPath.empty()) {
            printf("[IPDWrite] Warning: chunk has global faces but no glbPath provided\n");
        } else if (!fs::exists(glbPath)) {
            printf("[IPDWrite] Warning: _GLB.PLM not found at %s — global faces not saved\n",
                   glbPath.c_str());
        } else {
            std::vector<uint8_t> glbBuf;
            if (!ReadFile(glbPath, glbBuf)) return false;
            std::array<uint32_t, 8> origHash = CalculateSHA256(glbBuf);
            int n = PLMWrite::UpdateMeshStructure(glbBuf, 0, chunk, true, [](std::vector<uint8_t>& b, int p, int d) { PLMWrite::RelocatePLMOffsets(b, p, d, 0); });
            std::array<uint32_t, 8> newHash = CalculateSHA256(glbBuf);
            if (origHash != newHash) {
                if (!WriteFileAtomic(glbPath, glbBuf)) return false;
                totalPatched += n;
                anyWritten = true;
                GlobalCache::Get().Invalidate(glbPath);
                printf("[IPDWrite] Patched %d global face(s) in %s\n", n, glbPath.c_str());
            }
        }
    }

    if (outPatchedCount) *outPatchedCount = totalPatched;
    if (outFilesWritten) *outFilesWritten = anyWritten;
    return true;
}

// ---------------------------------------------------------------------------
// Validate: structural integrity checks (stub)
// ---------------------------------------------------------------------------
std::vector<std::string> IPDWrite::Validate(const ParsedChunk& chunk) {
    std::vector<std::string> warnings;
    
    // Future checks (once we have buffer size):
    // - File size > 256 KB (patched engine limit)
    
    for (size_t oIdx = 0; oIdx < chunk.objects.size(); ++oIdx) {
        const auto& obj = chunk.objects[oIdx];
        const auto& texList = obj.isGlobal ? chunk.globalTexNames : chunk.localTexNames;
        
        for (size_t mIdx = 0; mIdx < obj.meshes.size(); ++mIdx) {
            const auto& mesh = obj.meshes[mIdx];
            
            if (mesh.vx.size() == 0 && mesh.faces.size() > 0) {
                warnings.push_back("Object '" + obj.name + "' mesh " + std::to_string(mIdx) + 
                                   " has 0 vertices but " + std::to_string(mesh.faces.size()) + " faces.");
            }
            
            for (size_t fIdx = 0; fIdx < mesh.faces.size(); ++fIdx) {
                const auto& face = mesh.faces[fIdx];
                std::string faceId = "Object '" + obj.name + "' mesh " + std::to_string(mIdx) + " face " + std::to_string(fIdx);
                
                // Texture Assignment Integrity
                if (!face.texName.empty()) {
                    bool found = false;
                    for (const auto& tname : texList) {
                        if (tname == face.texName) { found = true; break; }
                    }
                    if (!found) {
                        warnings.push_back(faceId + " requires texture '" + face.texName + 
                                           "' which is not present in the " + (obj.isGlobal ? "global" : "local") + " texture list.");
                    }
                }
                
                // Vertex bounds check
                int numVerts = (face.v[3] == 0xFF) ? 3 : 4;
                for (int i = 0; i < numVerts; ++i) {
                    if (face.v[i] >= mesh.vx.size()) {
                        warnings.push_back(faceId + " uses out-of-bounds vertex index " + 
                                           std::to_string(face.v[i]) + " (max " + std::to_string(mesh.vx.size() - 1) + ").");
                    }
                }
                
                // UV bounds check
                for (int i = 0; i < numVerts; ++i) {
                    if (face.uv[i][0] < 0.0f || face.uv[i][0] > 1.0f || 
                        face.uv[i][1] < 0.0f || face.uv[i][1] > 1.0f) {
                        warnings.push_back(faceId + " has UV coordinates outside [0.0, 1.0].");
                    }
                }
            }
        }
    }
    
    return warnings;
}

// ---------------------------------------------------------------------------
// RelocateIPDOffsets: stub (future: size-changing edits)
// ---------------------------------------------------------------------------
void IPDWrite::RelocateIPDOffsets(std::vector<uint8_t>& buf,
                                   int insertionPoint,
                                   int delta) {
    if (buf.size() < sizeof(IPD_FILE_HEADER)) return;
    
    auto adjust = [&](int32_t* offset) {
        if (*offset >= insertionPoint) *offset += delta;
    };

    IPD_FILE_HEADER* hdr = (IPD_FILE_HEADER*)buf.data();
    
    adjust(&hdr->plm_offset);
    adjust(&hdr->obj_name_offset);
    adjust(&hdr->obj_data_offset);
    adjust(&hdr->unkdata_offset);
    
    // Check for collision header (if space between main header and obj_name table == 308 bytes)
    // Note: obj_name_offset was just adjusted, so we should use the original value to check the gap,
    // or just assume the collision header is at sizeof(IPD_FILE_HEADER) and check if it fits before the new obj_name_offset.
    // Actually, since obj_name_offset is > 0 and normally 84 or 392, it's safer to just check the *original* gap.
    // But even if we use the adjusted obj_name_offset, if we inserted AFTER the header, the collision block is at 84.
    // If insertionPoint <= 84, the collision block shifts, but its relative pointers don't change!
    // Wait, if insertion is inside the 308 bytes, or before it, it's complex. 
    // Usually insertion is in pos_groups or PLM. So we can safely look at offset 84.
    uint32_t collBase = sizeof(IPD_FILE_HEADER);
    // If original obj_name_offset (before adjustment) - collBase == sizeof(IPD_COLL_HEADER) (308)
    uint32_t origObjName = hdr->obj_name_offset; 
    if (origObjName >= insertionPoint) origObjName -= delta; // recover original
    
    if (origObjName - collBase == sizeof(IPD_COLL_HEADER)) {
        IPD_COLL_HEADER* collHdr = (IPD_COLL_HEADER*)(buf.data() + collBase);
        
        // Pointers in IPD_COLL_HEADER are relative to the start of the collision header (collBase).
        // If absolute offset (collBase + ptr) >= insertionPoint, adjust the ptr by delta.
        auto adjustRelative = [&](int32_t* ptr) {
            if (*ptr > 0) {
                if (collBase + *ptr >= (uint32_t)insertionPoint) {
                    *ptr += delta;
                }
            }
        };
        
        adjustRelative(&collHdr->ptr_splitVertices);
        adjustRelative(&collHdr->ptr_surfaces);
        adjustRelative(&collHdr->ptr_subcells);
        adjustRelative(&collHdr->ptr_unkBlock3);
        adjustRelative(&collHdr->ptr_grid);
        adjustRelative(&collHdr->ptr_block5);
        adjustRelative(&collHdr->ptr_block6);
        adjustRelative(&collHdr->ptr_unk7);
    }
    
    // Relocate POS headers
    if (hdr->obj_data_offset > 0 && hdr->obj_data_offset + hdr->pos_num * sizeof(IPD_POS_HEADER) <= buf.size()) {
        IPD_POS_HEADER* posHdr = (IPD_POS_HEADER*)(buf.data() + hdr->obj_data_offset);
        for (int i = 0; i < hdr->pos_num; ++i) {
            adjust(&posHdr[i].data_offset);
            adjust(&posHdr[i].unk1_offset);
            adjust(&posHdr[i].unk2_offset);
        }
    }
    
    // Relocate embedded PLM
    if (hdr->plm_offset > 0) {
        // We must pass the relative insertion point for the PLM block.
        // If the insertion happened BEFORE the PLM block, the PLM block itself was shifted,
        // but its internal absolute offsets (which are relative to the start of the PLM block) 
        // do not change relative to the PLM start! They only change if insertion was INSIDE the PLM block.
        // So we subtract plm_offset from insertionPoint.
        int plmInsertionPoint = insertionPoint - hdr->plm_offset;
        
        // Wait, if plmInsertionPoint < 0, it means insertion was before the PLM block.
        // In that case, we don't need to adjust offsets inside the PLM block (they are relative to plmBase).
        // BUT wait, is RelocatePLMOffsets expecting an absolute file insertion point or a PLM-relative one?
        // It expects PLM-relative.
        if (plmInsertionPoint > 0) {
            PLMWrite::RelocatePLMOffsets(buf, plmInsertionPoint, delta, hdr->plm_offset);
        }
    }
}
