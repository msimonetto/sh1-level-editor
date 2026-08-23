#include "formats/PLMParse.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace {
// Read a little-endian int16 pair at buf[offset] → (x,y)
inline void ReadI16Pair(const uint8_t* buf, int offset, int16_t& x, int16_t& y) {
    memcpy(&x, buf + offset,     2);
    memcpy(&y, buf + offset + 2, 2);
}
inline void ReadI16(const uint8_t* buf, int offset, int16_t& z) {
    memcpy(&z, buf + offset, 2);
}
} // namespace

void PLMParse::ComputeLocalVertex(int16_t px, int16_t py, int16_t pz, float& x, float& y, float& z) {
    x =  (float)px * IPD_SCALE;
    y = -(float)py * IPD_SCALE;
    z = -(float)pz * IPD_SCALE;
}

void PLMParse::ApplyWorldTransform(float lx, float ly, float lz,
                                  const float rot[3][3],
                                  float tx, float ty, float tz,
                                  float& wx, float& wy, float& wz) {
    wx = rot[0][0]*lx + rot[0][1]*ly + rot[0][2]*lz + tx;
    wy = rot[1][0]*lx + rot[1][1]*ly + rot[1][2]*lz + ty;
    wz = rot[2][0]*lx + rot[2][1]*ly + rot[2][2]*lz + tz;
}

bool PLMParse::ParseAndPlaceObject(const uint8_t*                  buf,
                          size_t                          bufSize,
                          int                             plmBase,
                          const PLM_OBJ_HEADER*           objHdr,
                          const std::vector<std::string>& texNames,
                          bool                            isGlobal,
                          float                           worldTx,
                          float                           worldTy,
                          float                           worldTz,
                          float                           rot[3][3],
                          const IPD_OBJ_DATA*             rawDta,
                          RenderObject&                   outObj)
{
    outObj.isGlobal = isGlobal;
    char nameBuf[9] = {0};
    memcpy(nameBuf, objHdr->name, 8);
    outObj.name = std::string(nameBuf);

    outObj.bounds = {{999999.0f, 999999.0f, 999999.0f}, {-999999.0f, -999999.0f, -999999.0f}};

    if (rawDta) {
        outObj.rawTx = rawDta->tx;
        outObj.rawTy = rawDta->ty;
        outObj.rawTz = rawDta->tz;
        for (int r = 0; r < 3; r++) {
            const int16_t* row = (r==0) ? &rawDta->rt11 :
                                 (r==1) ? &rawDta->rt21 : &rawDta->rt31;
            outObj.rt[r][0] = row[0];
            outObj.rt[r][1] = row[1];
            outObj.rt[r][2] = row[2];
        }
    }

    int dataOff = plmBase + objHdr->data_offset;
    if (dataOff + (int)(objHdr->mesh_num * sizeof(PLM_DATA_HEADER)) > (int)bufSize) {
        printf("[PLMParse] Object '%s': data_offset out of bounds\n", outObj.name.c_str());
        return false;
    }

    for (int m = 0; m < objHdr->mesh_num; ++m) {
        int dataHdrOff = dataOff + m * (int)sizeof(PLM_DATA_HEADER);
        if (dataHdrOff + (int)sizeof(PLM_DATA_HEADER) > (int)bufSize) break;

        const PLM_DATA_HEADER* dh = (const PLM_DATA_HEADER*)(buf + dataHdrOff);

        int packOff  = plmBase + dh->pack_offset;
        int vxyOff   = plmBase + dh->vert_xy_offset;
        int vzOff    = plmBase + dh->vert_z_offset;

        // Bounds checks
        bool packOk = packOff + (int)(dh->pack_num * sizeof(PLM_PACK_HEADER)) <= (int)bufSize;
        bool vxyOk  = vxyOff  + (int)(dh->vert_num * 4)                       <= (int)bufSize;
        bool vzOk   = vzOff   + (int)(dh->vert_num * 2)                       <= (int)bufSize;
        if (!packOk || !vxyOk || !vzOk) {
            printf("[PLMParse] Object '%s' mesh %d: data out of bounds\n", outObj.name.c_str(), m);
            continue;
        }

        RenderMesh mesh;
        mesh.vx.resize(dh->vert_num);
        mesh.vy.resize(dh->vert_num);
        mesh.vz.resize(dh->vert_num);

        // Build local-space vertices, apply world transform, store as world coords
        for (int k = 0; k < dh->vert_num; ++k) {
            int16_t px, py, pz;
            ReadI16Pair(buf, vxyOff + k * 4, px, py);
            ReadI16(buf, vzOff + k * 2, pz);

            float lx, ly, lz;
            ComputeLocalVertex(px, py, pz, lx, ly, lz);

            float wx, wy, wz;
            ApplyWorldTransform(lx, ly, lz, rot, worldTx, worldTy, worldTz, wx, wy, wz);

            mesh.vx[k] = wx;
            mesh.vy[k] = wy;
            mesh.vz[k] = wz;

            outObj.bounds.min.x = std::min(outObj.bounds.min.x, wx);
            outObj.bounds.min.y = std::min(outObj.bounds.min.y, wy);
            outObj.bounds.min.z = std::min(outObj.bounds.min.z, wz);
            outObj.bounds.max.x = std::max(outObj.bounds.max.x, wx);
            outObj.bounds.max.y = std::max(outObj.bounds.max.y, wy);
            outObj.bounds.max.z = std::max(outObj.bounds.max.z, wz);
        }

        // Determine texture dimensions for UV normalisation
        const float tw = 256.0f;
        const float th = 256.0f;

        // Build faces
        for (int p = 0; p < dh->pack_num; ++p) {
            int pkOff = packOff + p * (int)sizeof(PLM_PACK_HEADER);
            if (pkOff + (int)sizeof(PLM_PACK_HEADER) > (int)bufSize) break;

            const PLM_PACK_HEADER* pk = (const PLM_PACK_HEADER*)(buf + pkOff);
            bool isQuad = (pk->faces_3 != 0xFF);

            // Decode tex_num (low 7 bits), palette row from CBA
            uint8_t texNum    = pk->tex_num_and_unk2_byte & 0x7F;
            uint8_t palRow    = (uint8_t)((pk->cba & 0x7FC0) >> 6);
            uint16_t cbaRaw   = pk->cba;

            // Validate vertex indices
            uint8_t maxIdx = std::max({pk->faces_0, pk->faces_1, pk->faces_2,
                                       isQuad ? pk->faces_3 : (uint8_t)0});
            if (maxIdx >= dh->vert_num) {
                printf("[PLMParse] Object '%s' mesh %d pack %d: vertex index %d >= vert_num %d\n",
                       outObj.name.c_str(), m, p, maxIdx, dh->vert_num);
                continue;
            }

            // UV bias (port of coordinate_math)
            uint8_t uArr[4] = { pk->u0, pk->u1, pk->u2, pk->u3 };
            uint8_t vArr[4] = { pk->v0, pk->v1, pk->v2, pk->v3 };
            int count = isQuad ? 4 : 3;

            uint8_t maxU = 0, minU = 255, maxV = 0, minV = 255;
            for (int i = 0; i < count; ++i) {
                maxU = std::max(maxU, uArr[i]);
                minU = std::min(minU, uArr[i]);
                maxV = std::max(maxV, vArr[i]);
                minV = std::min(minV, vArr[i]);
            }

            auto biasU = [&](uint8_t u) -> float {
                return (u == maxU && maxU > minU) ? (float)(u + 1) : (float)u;
            };
            auto biasV = [&](uint8_t v) -> float {
                return (v == maxV && maxV > minV) ? (float)(v + 1) : (float)v;
            };

            float fu0 = biasU(pk->u0) / tw;  float fv0 = biasV(pk->v0) / th;
            float fu1 = biasU(pk->u1) / tw;  float fv1 = biasV(pk->v1) / th;
            float fu2 = biasU(pk->u2) / tw;  float fv2 = biasV(pk->v2) / th;
            float fu3 = biasU(pk->u3) / tw;  float fv3 = biasV(pk->v3) / th;

            RenderFace face;
            face.texNum    = texNum;
            if (texNum != 0x7F && texNum < texNames.size()) {
                face.texName = texNames[texNum];
            } else {
                face.texName = "";
            }
            face.paletteRow = palRow;
            face.cbaRaw    = cbaRaw;

            // Store raw UV bytes before bias/normalisation for write-back round-trip
            face.rawU[0] = pk->u0; face.rawU[1] = pk->u1;
            face.rawU[2] = pk->u2; face.rawU[3] = pk->u3;
            face.rawV[0] = pk->v0; face.rawV[1] = pk->v1;
            face.rawV[2] = pk->v2; face.rawV[3] = pk->v3;

            // Preserve PS1 hardware flags/normals
            face.unk1 = pk->unk1;
            face.origTexByte = pk->tex_num_and_unk2_byte;
            face.normals[0] = pk->normals_0;
            face.normals[1] = pk->normals_1;
            face.normals[2] = pk->normals_2;
            face.normals[3] = pk->normals_3;

            // Record canonical write-back address
            face.addr.plmObjectName = outObj.name;
            face.addr.meshIdx       = m;
            face.addr.packIdx       = p;
            face.addr.isGlobal      = isGlobal;
            face.addr.packRawOffset = pkOff; // absolute offset within srcBuf

            if (isQuad) {
                // Quad winding: (v0,v2,v3,v1)
                face.v[0] = pk->faces_0; face.uv[0][0] = fu0; face.uv[0][1] = fv0;
                face.v[1] = pk->faces_2; face.uv[1][0] = fu2; face.uv[1][1] = fv2;
                face.v[2] = pk->faces_3; face.uv[2][0] = fu3; face.uv[2][1] = fv3;
                face.v[3] = pk->faces_1; face.uv[3][0] = fu1; face.uv[3][1] = fv1;
            } else {
                // Triangle winding: (v2,v1,v0)
                face.v[0] = pk->faces_2; face.uv[0][0] = fu2; face.uv[0][1] = fv2;
                face.v[1] = pk->faces_1; face.uv[1][0] = fu1; face.uv[1][1] = fv1;
                face.v[2] = pk->faces_0; face.uv[2][0] = fu0; face.uv[2][1] = fv0;
                face.v[3] = 0xFF;
            }

            mesh.faces.push_back(face);
        }

        outObj.meshes.push_back(std::move(mesh));
    }

    return !outObj.meshes.empty();
}

bool PLMParse::ParseGlbFile(const std::string&             glbPath,
                              std::vector<RenderObject>&     outObjects,
                              std::vector<std::string>&      outTexNames,
                              std::vector<IPDParse::GlbObjectInfo>& outInfo)
{
    FILE* f = fopen(glbPath.c_str(), "rb");
    if (!f) {
        printf("[PLMParse::ParseGlbFile] Cannot open: %s\n", glbPath.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf((size_t)fileSize);
    if (fread(buf.data(), 1, fileSize, f) != (size_t)fileSize) {
        fclose(f);
        printf("[PLMParse::ParseGlbFile] Read error: %s\n", glbPath.c_str());
        return false;
    }
    fclose(f);

    if (buf.size() < sizeof(PLM_FILE_HEADER)) {
        printf("[PLMParse::ParseGlbFile] File too small: %s\n", glbPath.c_str());
        return false;
    }

    const PLM_FILE_HEADER* hdr = (const PLM_FILE_HEADER*)buf.data();
    if (hdr->id != 0x0630) {
        printf("[PLMParse::ParseGlbFile] Bad PLM magic 0x%04X: %s\n", hdr->id, glbPath.c_str());
        return false;
    }

    // Texture name table
    outTexNames.clear();
    int texBase = (int)hdr->tex_name_offset;
    for (int i = 0; i < hdr->tex_num; ++i) {
        int off = texBase + i * 24;
        if (off + 24 > (int)buf.size()) break;
        char name[25] = {0};
        memcpy(name, buf.data() + off, 24);
        outTexNames.push_back(std::string(name));
    }

    // Identity rotation — no world transform applied
    float identRot[3][3] = {{1.f,0.f,0.f},{0.f,1.f,0.f},{0.f,0.f,1.f}};

    outObjects.clear();
    outInfo.clear();

    int objBase = (int)hdr->obj_start_offset;
    for (int i = 0; i < hdr->obj_num; ++i) {
        int off = objBase + i * (int)sizeof(PLM_OBJ_HEADER);
        if (off + (int)sizeof(PLM_OBJ_HEADER) > (int)buf.size()) break;

        const PLM_OBJ_HEADER* oh = (const PLM_OBJ_HEADER*)(buf.data() + off);

        RenderObject obj;
        ParseAndPlaceObject(buf.data(), buf.size(),
                            /*plmBase=*/0,
                            oh,
                            outTexNames,
                            /*isGlobal=*/true,
                            /*worldTx=*/0.f, /*worldTy=*/0.f, /*worldTz=*/0.f,
                            identRot,
                            /*rawDta=*/nullptr,
                            obj);

        // Count total packs (faces) across all submeshes
        int packCount = 0;
        for (const auto& mesh : obj.meshes)
            packCount += (int)mesh.faces.size();

        IPDParse::GlbObjectInfo info;
        info.name       = obj.name;
        info.mesh_id    = i;
        info.pack_count = packCount;

        outObjects.push_back(std::move(obj));
        outInfo.push_back(info);
    }

    printf("[PLMParse::ParseGlbFile] Loaded %d objects, %d textures from %s\n",
           (int)outObjects.size(), (int)outTexNames.size(), glbPath.c_str());
    return !outObjects.empty();
}
