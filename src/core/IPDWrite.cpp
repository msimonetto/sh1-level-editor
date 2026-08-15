#include "core/IPDWrite.h"
#include "core/structs.h"
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
// EncodeUVs: reverse bias + denormalise float UVs → uint8
// ---------------------------------------------------------------------------
// Reverse the UV bias and re-encode normalised float UVs to uint8.
// The parser normalises as: uv[i][0] = biasU(rawU) / 256,  uv[i][1] = biasV(rawV) / 256
// There is NO Y-flip applied by IPDParse on the V channel.
//
// Encode:
//   rawU = uv[i][0] * 256 - bias
//   rawV = uv[i][1] * 256 - bias
// bias: the original rawU/rawV arrays detect which vertex had +1 bias applied.
// Values are rounded to nearest integer and clamped to [0, 255].
void IPDWrite::EncodeUVs(const RenderFace& face, uint8_t outU[4], uint8_t outV[4]) {
    bool isQuad = (face.v[3] != 0xFF);
    int count = isQuad ? 4 : 3;

    // We work in the same winding order as stored in face.uv / face.v
    // but the rawU/rawV are in original pack order (u0..u3, v0..v3).
    // The mapping between pack-order and face-winding-order is:
    //   quad:  face index 0=pk0, 1=pk2, 2=pk3, 3=pk1   (winding: v0,v2,v3,v1)
    //   tri:   face index 0=pk2, 1=pk1, 2=pk0           (winding: v2,v1,v0)
    // To re-encode back to pack order we invert these mappings.

    // faceToPackIdx[isQuad][faceIdx] -> packIdx
    static const int faceToPackQuad[4] = {0, 2, 3, 1}; // face->pack for quad
    static const int faceToPackTri[3]  = {2, 1, 0};    // face->pack for tri

    // Step 1: denormalise face UV coords back to float pack-order array
    float pkU[4] = {}, pkV[4] = {};
    for (int fi = 0; fi < count; fi++) {
        int pi = isQuad ? faceToPackQuad[fi] : faceToPackTri[fi];
        pkU[pi] = face.uv[fi][0] * 256.0f; // undo divide by 256
        pkV[pi] = face.uv[fi][1] * 256.0f; // undo divide by 256 (parser does NOT Y-flip V)
    }

    // Step 2: detect which pack vertices had bias applied by finding the max
    // raw value strictly greater than all others (same logic as the parser).
    uint8_t maxRawU = 0, minRawU = 255, maxRawV = 0, minRawV = 255;
    for (int i = 0; i < count; i++) {
        maxRawU = std::max(maxRawU, face.rawU[i]);
        minRawU = std::min(minRawU, face.rawU[i]);
        maxRawV = std::max(maxRawV, face.rawV[i]);
        minRawV = std::min(minRawV, face.rawV[i]);
    }
    bool hasBiasU = (maxRawU > minRawU);
    bool hasBiasV = (maxRawV > minRawV);

    // Step 3: reverse bias, round to nearest int, clamp [0, 255]
    for (int i = 0; i < 4; i++) {
        float fu = pkU[i];
        float fv = pkV[i];
        // Only subtract bias from the pack vertex that originally had it
        // (rawU[i] == maxRawU and bias applies → the biased float is maxRawU+1)
        if (hasBiasU && face.rawU[i] == maxRawU) fu -= 1.0f;
        if (hasBiasV && face.rawV[i] == maxRawV) fv -= 1.0f;

        outU[i] = (uint8_t)std::clamp((int)std::lroundf(fu), 0, 255);
        outV[i] = (uint8_t)std::clamp((int)std::lroundf(fv), 0, 255);
    }
    // For triangle: pack indices 3 are unused — copy from pack index 0 to
    // keep the bytes consistent with the original (they are irrelevant to rendering)
    if (!isQuad) {
        outU[3] = face.rawU[3];
        outV[3] = face.rawV[3];
    }
}

// ---------------------------------------------------------------------------
// EncodeFaceAtOffset: write all fields (UV/CBA/texnum, vertices) into working buffer
// ---------------------------------------------------------------------------
void IPDWrite::EncodeFaceAtOffset(std::vector<uint8_t>& buf, const RenderFace& face, int off) {
    if (off < 0 || off + (int)sizeof(PLM_PACK_HEADER) > (int)buf.size()) {
        return;
    }

    uint8_t encU[4], encV[4];
    EncodeUVs(face, encU, encV);

    PLM_PACK_HEADER* pk = (PLM_PACK_HEADER*)(buf.data() + off);
    
    pk->u0 = encU[0];
    pk->v0 = encV[0];
    pk->u1 = encU[1];
    pk->v1 = encV[1];
    pk->u2 = encU[2];
    pk->v2 = encV[2];
    pk->u3 = encU[3];
    pk->v3 = encV[3];

    // Re-encode CBA: patch palette row into bits [14:6], preserve all other bits from the original face
    pk->cba = (face.cbaRaw & ~0x7FC0u) | (((uint16_t)face.paletteRow & 0xFF) << 6);

    // Re-encode texnum: patch low 7 bits, preserve bit 7 (unk2) from the original face
    pk->tex_num_and_unk2_byte = (face.origTexByte & 0x80) | (face.texNum & 0x7F);

    // Write back preserved hardware flags and normals
    pk->unk1 = face.unk1;
    pk->normals_0 = face.normals[0];
    pk->normals_1 = face.normals[1];
    pk->normals_2 = face.normals[2];
    pk->normals_3 = face.normals[3];

    // Update vertex indices
    pk->faces_0 = face.v[0];
    if (face.v[3] != 0xFF) { // Quad (v0, v2, v3, v1 winding in RenderFace)
        pk->faces_2 = face.v[1];
        pk->faces_3 = face.v[2];
        pk->faces_1 = face.v[3];
    } else { // Triangle (v2, v1, v0 winding in RenderFace)
        pk->faces_2 = face.v[0];
        pk->faces_1 = face.v[1];
        pk->faces_0 = face.v[2];
        pk->faces_3 = 0xFF;
    }
}

// ---------------------------------------------------------------------------
// UpdateMeshStructure: dynamic resizing and encoding
// ---------------------------------------------------------------------------
int IPDWrite::UpdateMeshStructure(std::vector<uint8_t>& buf,
                                   const ParsedChunk& chunk,
                                   bool isGlobalFile) {
    int count = 0;
    if (buf.size() < sizeof(IPD_FILE_HEADER) && !isGlobalFile) return 0;
    if (buf.size() < sizeof(PLM_FILE_HEADER) && isGlobalFile) return 0;

    int plmBase = isGlobalFile ? 0 : ((IPD_FILE_HEADER*)buf.data())->plm_offset;
    if (plmBase <= 0 || plmBase + (int)sizeof(PLM_FILE_HEADER) > (int)buf.size()) return 0;

    PLM_FILE_HEADER* plmHdr = (PLM_FILE_HEADER*)(buf.data() + plmBase);
    if (plmHdr->id != 0x0630) return 0;

    int objBase = plmBase + plmHdr->obj_start_offset;
    for (int i = 0; i < plmHdr->obj_num; ++i) {
        int off = objBase + i * (int)sizeof(PLM_OBJ_HEADER);
        if (off + (int)sizeof(PLM_OBJ_HEADER) > (int)buf.size()) break;
        PLM_OBJ_HEADER* oh = (PLM_OBJ_HEADER*)(buf.data() + off);
        char name[9] = {0};
        memcpy(name, oh->name, 8);
        std::string objName(name);

        const RenderObject* targetObj = nullptr;
        for (const auto& ro : chunk.objects) {
            if (ro.name == objName && ro.isGlobal == isGlobalFile) {
                targetObj = &ro;
                break;
            }
        }
        if (!targetObj) continue;

        int dataOff = plmBase + oh->data_offset;
        for (int m = 0; m < oh->mesh_num; ++m) {
            if (m >= (int)targetObj->meshes.size()) break;
            const RenderMesh& rmesh = targetObj->meshes[m];
            
            int dataHdrOff = dataOff + m * (int)sizeof(PLM_DATA_HEADER);
            if (dataHdrOff + (int)sizeof(PLM_DATA_HEADER) > (int)buf.size()) break;
            PLM_DATA_HEADER* dh = (PLM_DATA_HEADER*)(buf.data() + dataHdrOff);

            int origVertNum = dh->vert_num;
            int newVertNum = (int)rmesh.vx.size();
            int origPackNum = dh->pack_num;
            int newPackNum = (int)rmesh.faces.size();

            // Shift pack
            if (newPackNum != origPackNum) {
                int packDiff = (newPackNum - origPackNum) * (int)sizeof(PLM_PACK_HEADER);
                int packInsertPoint = plmBase + dh->pack_offset + origPackNum * (int)sizeof(PLM_PACK_HEADER);
                if (packDiff > 0) {
                    buf.insert(buf.begin() + packInsertPoint, packDiff, 0);
                } else if (packDiff < 0) {
                    buf.erase(buf.begin() + packInsertPoint + packDiff, buf.begin() + packInsertPoint);
                }
                if (packDiff != 0) {
                    if (isGlobalFile) RelocatePLMOffsets(buf, packInsertPoint, packDiff, 0);
                    else RelocateIPDOffsets(buf, packInsertPoint, packDiff);
                }
                dh = (PLM_DATA_HEADER*)(buf.data() + dataHdrOff);
                dh->pack_num = newPackNum;
            }

            // Shift vert_xy
            if (newVertNum != origVertNum) {
                int vxyDiff = (newVertNum - origVertNum) * 4;
                int vxyInsertPoint = plmBase + dh->vert_xy_offset + origVertNum * 4;
                if (vxyDiff > 0) {
                    buf.insert(buf.begin() + vxyInsertPoint, vxyDiff, 0);
                } else if (vxyDiff < 0) {
                    buf.erase(buf.begin() + vxyInsertPoint + vxyDiff, buf.begin() + vxyInsertPoint);
                }
                if (vxyDiff != 0) {
                    if (isGlobalFile) RelocatePLMOffsets(buf, vxyInsertPoint, vxyDiff, 0);
                    else RelocateIPDOffsets(buf, vxyInsertPoint, vxyDiff);
                }
                dh = (PLM_DATA_HEADER*)(buf.data() + dataHdrOff);
                
                // Shift vert_z
                int vzDiff = (newVertNum - origVertNum) * 2;
                int vzInsertPoint = plmBase + dh->vert_z_offset + origVertNum * 2;
                if (vzDiff > 0) {
                    buf.insert(buf.begin() + vzInsertPoint, vzDiff, 0);
                } else if (vzDiff < 0) {
                    buf.erase(buf.begin() + vzInsertPoint + vzDiff, buf.begin() + vzInsertPoint);
                }
                if (vzDiff != 0) {
                    if (isGlobalFile) RelocatePLMOffsets(buf, vzInsertPoint, vzDiff, 0);
                    else RelocateIPDOffsets(buf, vzInsertPoint, vzDiff);
                }
                dh = (PLM_DATA_HEADER*)(buf.data() + dataHdrOff);
                dh->vert_num = newVertNum;
            }

            // Encode Vertices
            for (int k = 0; k < newVertNum; ++k) {
                float wx = rmesh.vx[k];
                float wy = rmesh.vy[k];
                float wz = rmesh.vz[k];

                float tx = ((float)targetObj->rawTx + 10240.0f * (float)chunk.xPos) * (1.0f/256.0f);
                float ty = -((float)targetObj->rawTy) * (1.0f/256.0f);
                float tz = -((float)targetObj->rawTz + 10240.0f * (float)chunk.yPos) * (1.0f/256.0f);
                
                float dx = wx - tx;
                float dy = wy - ty;
                float dz = wz - tz;

                float rot[3][3];
                rot[0][0] =  targetObj->rt[0][0] / 4096.0f;
                rot[0][1] = -targetObj->rt[0][1] / 4096.0f;
                rot[0][2] = -targetObj->rt[0][2] / 4096.0f;
                rot[1][0] = -targetObj->rt[1][0] / 4096.0f;
                rot[1][1] =  targetObj->rt[1][1] / 4096.0f;
                rot[1][2] =  targetObj->rt[1][2] / 4096.0f;
                rot[2][0] = -targetObj->rt[2][0] / 4096.0f;
                rot[2][1] =  targetObj->rt[2][1] / 4096.0f;
                rot[2][2] =  targetObj->rt[2][2] / 4096.0f;

                float lx = rot[0][0]*dx + rot[1][0]*dy + rot[2][0]*dz;
                float ly = rot[0][1]*dx + rot[1][1]*dy + rot[2][1]*dz;
                float lz = rot[0][2]*dx + rot[1][2]*dy + rot[2][2]*dz;

                int16_t px = (int16_t)std::clamp((int)std::lroundf(lx * 256.0f), -32768, 32767);
                int16_t py = (int16_t)std::clamp((int)std::lroundf(-ly * 256.0f), -32768, 32767);
                int16_t pz = (int16_t)std::clamp((int)std::lroundf(-lz * 256.0f), -32768, 32767);

                int vxyOff = plmBase + dh->vert_xy_offset + k * 4;
                int vzOff = plmBase + dh->vert_z_offset + k * 2;
                memcpy(buf.data() + vxyOff, &px, 2);
                memcpy(buf.data() + vxyOff + 2, &py, 2);
                memcpy(buf.data() + vzOff, &pz, 2);
            }

            // Encode Faces
            for (int p = 0; p < newPackNum; ++p) {
                int pkOff = plmBase + dh->pack_offset + p * (int)sizeof(PLM_PACK_HEADER);
                EncodeFaceAtOffset(buf, rmesh.faces[p], pkOff);
                count++;
            }
        }
    }
    return count;
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

        int n = UpdateMeshStructure(ipdBuf, chunk, false);
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
            int n = UpdateMeshStructure(glbBuf, chunk, true);
            std::array<uint32_t, 8> newHash = CalculateSHA256(glbBuf);
            if (origHash != newHash) {
                if (!WriteFileAtomic(glbPath, glbBuf)) return false;
                totalPatched += n;
                anyWritten = true;
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
            RelocatePLMOffsets(buf, plmInsertionPoint, delta, hdr->plm_offset);
        }
    }
}

// ---------------------------------------------------------------------------
// RelocatePLMOffsets: stub (future: GLB PLM size-changing edits)
// ---------------------------------------------------------------------------
void IPDWrite::RelocatePLMOffsets(std::vector<uint8_t>& buf,
                                   int insertionPoint,
                                   int delta,
                                   int plmBase) {
    if (buf.size() < plmBase + sizeof(PLM_FILE_HEADER)) return;
    
    auto adjust = [&](int32_t* offset) {
        if (*offset >= insertionPoint) *offset += delta;
    };
    
    PLM_FILE_HEADER* hdr = (PLM_FILE_HEADER*)(buf.data() + plmBase);
    if (hdr->id != 0x0630) return;
    
    adjust(&hdr->tex_name_offset);
    adjust(&hdr->obj_start_offset);
    adjust(&hdr->unk_data_offset);
    
    if (hdr->obj_start_offset > 0 && hdr->obj_start_offset + hdr->obj_num * sizeof(PLM_OBJ_HEADER) <= buf.size() - plmBase) {
        PLM_OBJ_HEADER* objHdr = (PLM_OBJ_HEADER*)(buf.data() + plmBase + hdr->obj_start_offset);
        for (int i = 0; i < hdr->obj_num; ++i) {
            adjust(&objHdr[i].data_offset);
            
            if (objHdr[i].data_offset > 0 && objHdr[i].data_offset + objHdr[i].mesh_num * sizeof(PLM_DATA_HEADER) <= buf.size() - plmBase) {
                PLM_DATA_HEADER* dataHdr = (PLM_DATA_HEADER*)(buf.data() + plmBase + objHdr[i].data_offset);
                for (int m = 0; m < objHdr[i].mesh_num; ++m) {
                    adjust(&dataHdr[m].pack_offset);
                    adjust(&dataHdr[m].vert_xy_offset);
                    adjust(&dataHdr[m].vert_z_offset);
                    adjust(&dataHdr[m].normal_offset);
                    adjust(&dataHdr[m].end_offset);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// ResolveFaceOffset: re-derive packRawOffset from logical FaceAddress
// ---------------------------------------------------------------------------
int IPDWrite::ResolveFaceOffset(const std::vector<uint8_t>& buf,
                                 const FaceAddress& addr,
                                 int plmBase) {
    if (buf.size() < (size_t)(plmBase + (int)sizeof(PLM_FILE_HEADER))) return -1;

    const PLM_FILE_HEADER* plmHdr =
        (const PLM_FILE_HEADER*)(buf.data() + plmBase);
    if (plmHdr->id != 0x0630) return -1;

    // Find PLM_OBJ_HEADER by name
    int objBase = plmBase + plmHdr->obj_start_offset;
    for (int i = 0; i < plmHdr->obj_num; ++i) {
        int off = objBase + i * (int)sizeof(PLM_OBJ_HEADER);
        if (off + (int)sizeof(PLM_OBJ_HEADER) > (int)buf.size()) break;
        const PLM_OBJ_HEADER* oh = (const PLM_OBJ_HEADER*)(buf.data() + off);
        char name[9] = {0};
        memcpy(name, oh->name, 8);
        if (addr.plmObjectName != std::string(name)) continue;

        // Found object — navigate to mesh then pack
        int dataOff = plmBase + oh->data_offset;
        int dhOff = dataOff + addr.meshIdx * (int)sizeof(PLM_DATA_HEADER);
        if (dhOff + (int)sizeof(PLM_DATA_HEADER) > (int)buf.size()) return -1;
        const PLM_DATA_HEADER* dh = (const PLM_DATA_HEADER*)(buf.data() + dhOff);

        int packOff = plmBase + dh->pack_offset + addr.packIdx * (int)sizeof(PLM_PACK_HEADER);
        if (packOff + (int)sizeof(PLM_PACK_HEADER) > (int)buf.size()) return -1;
        return packOff;
    }
    return -1; // object not found
}
