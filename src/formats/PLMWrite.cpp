#include "formats/PLMWrite.h"
#include <algorithm>
#include <cmath>
#include <cstring>

//   rawV = uv[i][1] * 256 - bias
// bias: the original rawU/rawV arrays detect which vertex had +1 bias applied.
// Values are rounded to nearest integer and clamped to [0, 255].
void PLMWrite::EncodeUVs(const RenderFace& face, uint8_t outU[4], uint8_t outV[4]) {
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
void PLMWrite::EncodeFaceAtOffset(std::vector<uint8_t>& buf, const RenderFace& face, int off) {
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
int PLMWrite::UpdateMeshStructure(std::vector<uint8_t>& buf,
                                   int plmBase,
                                   const ParsedChunk& chunk,
                                   bool isGlobalFile,
                                   std::function<void(std::vector<uint8_t>&, int, int)> relocateFn) {
    int count = 0;
    if (buf.size() < sizeof(IPD_FILE_HEADER) && !isGlobalFile) return 0;
    if (buf.size() < sizeof(PLM_FILE_HEADER) && isGlobalFile) return 0;

    // plmBase is now passed in
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
                    if (relocateFn) relocateFn(buf, packInsertPoint, packDiff);
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
                    if (relocateFn) relocateFn(buf, vxyInsertPoint, vxyDiff);
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
                    if (relocateFn) relocateFn(buf, vzInsertPoint, vzDiff);
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

            bool structuralChange = (newVertNum != origVertNum) || (newPackNum != origPackNum);

            // Encode Faces
            for (int p = 0; p < newPackNum; ++p) {
                if (rmesh.faces[p].isDirty || structuralChange) {
                    int pkOff = plmBase + dh->pack_offset + p * (int)sizeof(PLM_PACK_HEADER);
                    EncodeFaceAtOffset(buf, rmesh.faces[p], pkOff);
                    count++;
                    const_cast<RenderFace&>(rmesh.faces[p]).isDirty = false;
                }
            }
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// RelocatePLMOffsets: stub (future: GLB PLM size-changing edits)
// ---------------------------------------------------------------------------
void PLMWrite::RelocatePLMOffsets(std::vector<uint8_t>& buf,
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
int PLMWrite::ResolveFaceOffset(const std::vector<uint8_t>& buf,
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

