/*
 * IPDParse.cpp — Full IPD + PLM geometry extractor
 *
 * Coordinate math is a faithful C++ port of coordinate_math:
 *   ps1_to_blender_vertex(x,y,z) = (-x*SCALE, -z*SCALE, -y*SCALE)
 *   loc_x = -(tx + MAP_MAX * x_pos) * SCALE
 *   loc_y = -(tz + MAP_MAX * y_pos) * SCALE
 *   loc_z = -(ty) * SCALE
 *
 * UV bias (from coordinate_math):
 *   u_final = u + 1  if (u == max_u AND max_u > min_u)  else  u
 *   v_final = v + 1  if (v == max_v AND max_v > min_v)  else  v
 *   uv_norm = (u_final / tw,  1.0 - v_final / th)
 *
 * Winding:
 *   Quad → vertices (v0,v2,v3,v1), UVs (uv0,uv2,uv3,uv1)
 *   Tri  → vertices (v2,v1,v0),   UVs (uv2,uv1,uv0)
 *
 * CBA decode:
 *   clut_y (palette row) = (cba & 0x7FC0) >> 6
 *   clut_x               = (cba & 0x003F) << 4   (stored in cbaRaw, not used for rendering)
 *
 * tex_num decode:
 *   tex_num  = tex_num_and_unk2_byte & 0x7F
 *   0x7F     = no-texture sentinel
 */

#include "core/IPDParse.h"
#include "core/structs.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <map>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// Read a little-endian int16 pair at buf[offset] → (x,y)
inline void ReadI16Pair(const uint8_t* buf, int offset, int16_t& x, int16_t& y) {
    memcpy(&x, buf + offset,     2);
    memcpy(&y, buf + offset + 2, 2);
}
inline void ReadI16(const uint8_t* buf, int offset, int16_t& z) {
    memcpy(&z, buf + offset, 2);
}

// ---------------------------------------------------------------------------
// ps1_to_blender_vertex (local mesh space, already scaled)
// SH1: Y-down, Z-forward  →  Blender/Raylib: Z-up, Y-forward
// ---------------------------------------------------------------------------
inline void ComputeLocalVertex(int16_t px, int16_t py, int16_t pz, float& x, float& y, float& z) {
    x =  (float)px * IPD_SCALE;
    y = -(float)py * IPD_SCALE;
    z = -(float)pz * IPD_SCALE;
}

// ---------------------------------------------------------------------------
// Apply world rotation + translation (pre-computed) to a local vertex.
// rot[row][col] are already /4096 floats.
// world = rot * local + translation
// ---------------------------------------------------------------------------
inline void ApplyWorldTransform(float lx, float ly, float lz,
                                  const float rot[3][3],
                                  float tx, float ty, float tz,
                                  float& wx, float& wy, float& wz) {
    wx = rot[0][0]*lx + rot[0][1]*ly + rot[0][2]*lz + tx;
    wy = rot[1][0]*lx + rot[1][1]*ly + rot[1][2]*lz + ty;
    wz = rot[2][0]*lx + rot[2][1]*ly + rot[2][2]*lz + tz;
}

// ---------------------------------------------------------------------------
// Compute world translation from IPD_OBJ_DATA.
// X is mapped positively, Y and Z are inverted.
// ---------------------------------------------------------------------------
void ComputeWorldTranslation(const IPD_OBJ_DATA& dta,
                              int8_t xPos, int8_t yPos,
                              float& tx, float& ty, float& tz) {
    tx =  ((float)dta.tx + IPD_MAP_MAX * (float)xPos) * IPD_SCALE;
    ty = -((float)dta.ty) * IPD_SCALE;
    tz = -((float)dta.tz + IPD_MAP_MAX * (float)yPos) * IPD_SCALE;
}

// ---------------------------------------------------------------------------
// Compute world rotation matrix from IPD_OBJ_DATA.
// For Raylib (Y-up) with a (x, -y, -z) mapping from PS1, the transformation
// matrix is diag(1, -1, -1). R_ray = S * R_ps1 * S.
// ---------------------------------------------------------------------------
void ComputeWorldRotation(const IPD_OBJ_DATA& dta, float rot[3][3]) {
    rot[0][0] =  dta.rt11 / 4096.0f;
    rot[0][1] = -dta.rt12 / 4096.0f;
    rot[0][2] = -dta.rt13 / 4096.0f;

    rot[1][0] = -dta.rt21 / 4096.0f;
    rot[1][1] =  dta.rt22 / 4096.0f;
    rot[1][2] =  dta.rt23 / 4096.0f;

    rot[2][0] = -dta.rt31 / 4096.0f;
    rot[2][1] =  dta.rt32 / 4096.0f;
    rot[2][2] =  dta.rt33 / 4096.0f;
}

// ---------------------------------------------------------------------------
// Parse one PLM_OBJ_HEADER's geometry and apply world transform.
// Returns false if the object is not found or data is malformed.
// ---------------------------------------------------------------------------
bool ParseAndPlaceObject(const uint8_t*                  buf,
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
        printf("[IPDParse] Object '%s': data_offset out of bounds\n", outObj.name.c_str());
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
            printf("[IPDParse] Object '%s' mesh %d: data out of bounds\n", outObj.name.c_str(), m);
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
        // Default 256×256; actual size is read at render time from Textures
        // We store normalised UVs [0,1] using tw=256, th=256.
        // Raylib shader will sample correctly regardless of actual GPU texture size.
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
                printf("[IPDParse] Object '%s' mesh %d pack %d: vertex index %d >= vert_num %d\n",
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
                // Quad winding: (v0,v2,v3,v1) — matches Blender bridge line 399
                face.v[0] = pk->faces_0; face.uv[0][0] = fu0; face.uv[0][1] = fv0;
                face.v[1] = pk->faces_2; face.uv[1][0] = fu2; face.uv[1][1] = fv2;
                face.v[2] = pk->faces_3; face.uv[2][0] = fu3; face.uv[2][1] = fv3;
                face.v[3] = pk->faces_1; face.uv[3][0] = fu1; face.uv[3][1] = fv1;
            } else {
                // Triangle winding: (v2,v1,v0) — matches Blender bridge line 404
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

} // anonymous namespace

// ---------------------------------------------------------------------------
// IPDParse::Parse
// ---------------------------------------------------------------------------
bool IPDParse::Parse(const std::string& ipdPath,
                       const std::string& workspaceDir,
                       ParsedChunk&       out)
{
    // --- Read IPD file into buffer ---
    FILE* f = fopen(ipdPath.c_str(), "rb");
    if (!f) {
        printf("[IPDParse] Cannot open: %s\n", ipdPath.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf((size_t)fileSize);
    if (fread(buf.data(), 1, fileSize, f) != (size_t)fileSize) {
        fclose(f);
        return false;
    }
    fclose(f);

    if (buf.size() < sizeof(IPD_FILE_HEADER) || buf[0] != 0x14) {
        printf("[IPDParse] Invalid IPD file: %s\n", ipdPath.c_str());
        return false;
    }

    // --- Extract chunk name and prefix from path ---
    size_t slash = ipdPath.find_last_of("/\\");
    std::string filename = (slash != std::string::npos) ? ipdPath.substr(slash + 1) : ipdPath;
    size_t dot = filename.find_last_of('.');
    out.chunkName = (dot != std::string::npos) ? filename.substr(0, dot) : filename;

    // Derive prefix using standard hex coordinate stripping logic
    out.chunkPrefix = DeriveChunkPrefix(out.chunkName);

    const IPD_FILE_HEADER* ipdHdr = (const IPD_FILE_HEADER*)buf.data();
    out.xPos = ipdHdr->x_pos;
    out.yPos = ipdHdr->y_pos;

    // -----------------------------------------------------------------------
    // Parse Collision Data (if present between file header and obj name table)
    // -----------------------------------------------------------------------
    int headerEnd = sizeof(IPD_FILE_HEADER);
    int gapSize = ipdHdr->obj_name_offset - headerEnd;
    if (gapSize == 308) {
        IPD_COLL_HEADER collHdr;
        memcpy(&collHdr, buf.data() + headerEnd, sizeof(IPD_COLL_HEADER));

        out.collision.hasCollision = true;
        out.collision.positionX = collHdr.positionX;
        out.collision.positionZ = collHdr.positionZ;
        out.collision.gridScale = collHdr.gridScale;
        out.collision.gridWidth = collHdr.gridWidth;
        out.collision.gridHeight = collHdr.gridHeight;

        // Pointers are relative to headerEnd
        int ptrSplitVerts = collHdr.ptr_splitVertices + headerEnd;
        int ptrSurfaces = collHdr.ptr_surfaces + headerEnd;
        int ptrSubcells = collHdr.ptr_subcells + headerEnd;
        int ptrGrid = collHdr.ptr_grid + headerEnd;
        int ptrBlock5 = collHdr.ptr_block5 + headerEnd;
        int ptrBlock6 = collHdr.ptr_block6 + headerEnd;

        // Read Split Vertices
        out.collision.splitVertices.resize(collHdr.splitVertexCount);
        if (ptrSplitVerts + collHdr.splitVertexCount * sizeof(IPD_COLL_SVECTOR) <= buf.size()) {
            for (int i = 0; i < collHdr.splitVertexCount; ++i) {
                IPD_COLL_SVECTOR sv;
                memcpy(&sv, buf.data() + ptrSplitVerts + i * sizeof(IPD_COLL_SVECTOR), sizeof(IPD_COLL_SVECTOR));
                out.collision.splitVertices[i] = { sv.x, sv.y, sv.z };
            }
        }

        // Read Surfaces
        out.collision.surfaces.resize(collHdr.surfaceCount);
        if (ptrSurfaces + collHdr.surfaceCount * sizeof(IPD_COLL_SURFACE) <= buf.size()) {
            for (int i = 0; i < collHdr.surfaceCount; ++i) {
                IPD_COLL_SURFACE s;
                memcpy(&s, buf.data() + ptrSurfaces + i * sizeof(IPD_COLL_SURFACE), sizeof(IPD_COLL_SURFACE));
                out.collision.surfaces[i] = { s.baseGroundHeight, s.tilt_flags };
            }
        }

        // Read Subcells
        out.collision.subcells.resize(collHdr.subcellCount);
        if (ptrSubcells + collHdr.subcellCount * sizeof(IPD_COLL_SUBCELL) <= buf.size()) {
            for (int i = 0; i < collHdr.subcellCount; ++i) {
                IPD_COLL_SUBCELL sc;
                memcpy(&sc, buf.data() + ptrSubcells + i * sizeof(IPD_COLL_SUBCELL), sizeof(IPD_COLL_SUBCELL));
                out.collision.subcells[i] = { sc.splitVertexIdx0, sc.splitVertexIdx1, sc.surfaceIdx0, sc.surfaceIdx1 };
            }
        }

        // Read Grid (int16_t start, end)
        int gridArea = collHdr.gridWidth * collHdr.gridHeight;
        out.collision.grid.resize(gridArea);
        if (ptrGrid + gridArea * 4 <= buf.size()) {
            for (int i = 0; i < gridArea; ++i) {
                int16_t start, end;
                memcpy(&start, buf.data() + ptrGrid + i * 4, 2);
                memcpy(&end, buf.data() + ptrGrid + i * 4 + 2, 2);
                out.collision.grid[i] = { start, end };
            }
        }

        // Read Block5 and Block6
        out.collision.block5.resize(collHdr.block5Count);
        if (ptrBlock5 + collHdr.block5Count <= buf.size()) {
            memcpy(out.collision.block5.data(), buf.data() + ptrBlock5, collHdr.block5Count);
        }
        
        out.collision.block6.resize(collHdr.block6Count);
        if (ptrBlock6 + collHdr.block6Count <= buf.size()) {
            memcpy(out.collision.block6.data(), buf.data() + ptrBlock6, collHdr.block6Count);
        }
    }

    // --- Parse obj_name table (local = flag 0, global = flag 1) ---
    struct ObjNameEntry {
        int32_t flag;
        char    name[9];
    };
    std::vector<ObjNameEntry> objNames;
    if (ipdHdr->obj_name_offset > 0 &&
        ipdHdr->obj_name_offset + ipdHdr->obj_num * (int)sizeof(IPD_OBJNAME_DATA) <= (int)buf.size())
    {
        for (int i = 0; i < ipdHdr->obj_num; ++i) {
            const IPD_OBJNAME_DATA* nd = (const IPD_OBJNAME_DATA*)(
                buf.data() + ipdHdr->obj_name_offset + i * sizeof(IPD_OBJNAME_DATA));
            ObjNameEntry e;
            e.flag = nd->flag;
            memset(e.name, 0, 9);
            memcpy(e.name, nd->name, 8);
            objNames.push_back(e);
        }
    }

    // --- Parse the local (embedded) PLM section ---
    if (ipdHdr->plm_offset <= 0 || ipdHdr->plm_offset >= (int)buf.size()) {
        printf("[IPDParse] PLM offset invalid in %s\n", out.chunkName.c_str());
        return false;
    }
    int plmBase = ipdHdr->plm_offset;
    if (plmBase + (int)sizeof(PLM_FILE_HEADER) > (int)buf.size()) return false;

    const PLM_FILE_HEADER* plmHdr = (const PLM_FILE_HEADER*)(buf.data() + plmBase);
    if (plmHdr->id != 0x0630) {
        printf("[IPDParse] PLM magic mismatch in %s (got 0x%04X)\n", out.chunkName.c_str(), plmHdr->id);
        return false;
    }

    // Local texture names
    out.localTexNames.clear();
    int texBase = plmBase + plmHdr->tex_name_offset;
    for (int i = 0; i < plmHdr->tex_num; ++i) {
        int off = texBase + i * 24;
        if (off + 24 > (int)buf.size()) break;
        char name[25] = {0};
        memcpy(name, buf.data() + off, 24);
        out.localTexNames.push_back(std::string(name));
    }

    // Build local PLM obj lookup: name → PLM_OBJ_HEADER offset
    std::map<std::string, int> localObjMap;
    int objBase = plmBase + plmHdr->obj_start_offset;
    for (int i = 0; i < plmHdr->obj_num; ++i) {
        int off = objBase + i * (int)sizeof(PLM_OBJ_HEADER);
        if (off + (int)sizeof(PLM_OBJ_HEADER) > (int)buf.size()) break;
        const PLM_OBJ_HEADER* oh = (const PLM_OBJ_HEADER*)(buf.data() + off);
        char name[9] = {0};
        memcpy(name, oh->name, 8);
        localObjMap[std::string(name)] = off;
    }

    // --- Optionally load the Global PLM (_GLB.PLM) ---
    // Flag=1 entries reference objects in {PREFIX}_GLB.PLM.
    // We only attempt this if at least one flag=1 entry exists.
    bool needsGlobal = false;
    for (const auto& e : objNames) {
        if (e.flag == 1) { needsGlobal = true; break; }
    }

    std::vector<uint8_t> glbBuf;
    std::map<std::string, int> globalObjMap;
    bool glbLoaded = false;

    if (needsGlobal) {
        // Try workspace/geometry/{PREFIX}_GLB.PLM
        std::string glbPath = workspaceDir + "/geometry/" + out.chunkPrefix + "_GLB.PLM";
        FILE* gf = fopen(glbPath.c_str(), "rb");
        if (!gf) {
            // Fallback: same directory as IPD
            std::string ipdDir = ipdPath.substr(0, slash != std::string::npos ? slash + 1 : 0);
            glbPath = ipdDir + out.chunkPrefix + "_GLB.PLM";
            gf = fopen(glbPath.c_str(), "rb");
        }
        if (gf) {
            fseek(gf, 0, SEEK_END);
            long gsz = ftell(gf);
            fseek(gf, 0, SEEK_SET);
            glbBuf.resize(gsz);
            if (fread(glbBuf.data(), 1, gsz, gf) == (size_t)gsz) {
                glbLoaded = true;
                printf("[IPDParse] Loaded GLB: %s\n", glbPath.c_str());
            }
            fclose(gf);
        }

        if (glbLoaded && glbBuf.size() >= sizeof(PLM_FILE_HEADER)) {
            const PLM_FILE_HEADER* gh = (const PLM_FILE_HEADER*)glbBuf.data();
            if (gh->id == 0x0630) {
                // Global texture names
                int gTexBase = gh->tex_name_offset;
                for (int i = 0; i < gh->tex_num; ++i) {
                    int off = gTexBase + i * 24;
                    if (off + 24 > (int)glbBuf.size()) break;
                    char name[25] = {0};
                    memcpy(name, glbBuf.data() + off, 24);
                    out.globalTexNames.push_back(std::string(name));
                }
                // Build global obj lookup
                int gObjBase = gh->obj_start_offset;
                for (int i = 0; i < gh->obj_num; ++i) {
                    int off = gObjBase + i * (int)sizeof(PLM_OBJ_HEADER);
                    if (off + (int)sizeof(PLM_OBJ_HEADER) > (int)glbBuf.size()) break;
                    const PLM_OBJ_HEADER* oh = (const PLM_OBJ_HEADER*)(glbBuf.data() + off);
                    char name[9] = {0};
                    memcpy(name, oh->name, 8);
                    globalObjMap[std::string(name)] = off;
                }
            } else {
                printf("[IPDParse] GLB PLM magic mismatch (0x%04X), skipping global objects\n", gh->id);
                glbLoaded = false;
            }
        } else if (needsGlobal) {
            printf("[IPDParse] Warning: chunk %s needs global PLM but '%s_GLB.PLM' not found\n",
                   out.chunkName.c_str(), out.chunkPrefix.c_str());
        }
    }

    // --- Iterate position groups and placed objects ---
    if (ipdHdr->obj_data_offset <= 0 ||
        ipdHdr->obj_data_offset >= (int)buf.size()) {
        printf("[IPDParse] obj_data_offset invalid\n");
        return false;
    }


    for (int gi = 0; gi < ipdHdr->pos_num; ++gi) {
        int posOff = ipdHdr->obj_data_offset + gi * (int)sizeof(IPD_POS_HEADER);
        if (posOff + (int)sizeof(IPD_POS_HEADER) > (int)buf.size()) break;

        const IPD_POS_HEADER* pos = (const IPD_POS_HEADER*)(buf.data() + posOff);
        if (pos->data_offset <= 0 || pos->data_offset >= (int)buf.size()) continue;

        for (int oi = 0; oi < pos->obj_num; ++oi) {
            int dtaOff = pos->data_offset + oi * (int)sizeof(IPD_OBJ_DATA);
            if (dtaOff + (int)sizeof(IPD_OBJ_DATA) > (int)buf.size()) break;

            const IPD_OBJ_DATA* dta = (const IPD_OBJ_DATA*)(buf.data() + dtaOff);
            int objId = dta->obj_id;
            if (objId < 0 || objId >= (int)objNames.size()) continue;

            const ObjNameEntry& nameEntry = objNames[objId];
            std::string objName(nameEntry.name);
            bool isGlobal = (nameEntry.flag == 1);

            // Compute world transform for this placement
            float worldTx, worldTy, worldTz;
            ComputeWorldTranslation(*dta, out.xPos, out.yPos, worldTx, worldTy, worldTz);
            float rot[3][3];
            ComputeWorldRotation(*dta, rot);

            // --- Look up the PLM object ---
            const std::map<std::string, int>& objMap = isGlobal ? globalObjMap : localObjMap;
            const std::vector<uint8_t>& srcBuf = isGlobal ? glbBuf : buf;
            const std::vector<std::string>& texNames = isGlobal ? out.globalTexNames : out.localTexNames;
            int srcPlmBase = isGlobal ? 0 : plmBase; // GLB file base is 0; IPD embedded PLM uses plmBase

            if (!isGlobal || glbLoaded) {
                auto it = objMap.find(objName);
                if (it == objMap.end()) {
                    // Object name not found in PLM (can happen for non-existent GLB references)
                    if (isGlobal) {
                        printf("[IPDParse] Global obj '%s' not found in GLB (skipping)\n", objName.c_str());
                    }
                    continue;
                }

                int objHdrOff = it->second;
                const PLM_OBJ_HEADER* objHdr = (const PLM_OBJ_HEADER*)(srcBuf.data() + objHdrOff);

                RenderObject robj;
                if (ParseAndPlaceObject(srcBuf.data(), srcBuf.size(),
                                         srcPlmBase, objHdr, texNames,
                                         isGlobal, worldTx, worldTy, worldTz, rot, dta,
                                         robj))
                {
                    robj.ipdDataOffset = dtaOff;
                    robj.ipdObjId = objId;
                    robj.ipdPosGroup = gi;
                    out.objects.push_back(std::move(robj));
                }
            }
        }
    }

    out.loaded = true;
    printf("[IPDParse] Parsed '%s': %zu objects, %zu local textures, %zu global textures\n",
           out.chunkName.c_str(), out.objects.size(),
           out.localTexNames.size(), out.globalTexNames.size());
    return true;
}

// ---------------------------------------------------------------------------
// IPDParse::BuildBatches
// Flatten all objects into per-(texNum, paletteRow) draw batches.
// Quads are triangulated into two triangles for Raylib DrawMesh.
// ---------------------------------------------------------------------------
void IPDParse::BuildBatches(ParsedChunk& chunk)
{
    // key = texName + "_" + paletteRow
    std::map<std::string, RenderBatch> batchMap;

    for (const auto& obj : chunk.objects) {
        for (const auto& mesh : obj.meshes) {
            for (const auto& face : mesh.faces) {
                std::string tName = face.texName;
                uint8_t pal       = face.paletteRow;
                std::string key   = tName + "_" + std::to_string(pal);

                auto& batch = batchMap[key];
                batch.texName    = tName;
                batch.paletteRow = pal;

                bool isQuad = (face.v[3] != 0xFF);
                int triCount = isQuad ? 2 : 1;

                // Triangle indices into face.v / face.uv
                // Quad → tri0=(0,1,2)  tri1=(0,2,3)
                static const int triV[2][3] = { {0,1,2}, {0,2,3} };

                for (int t = 0; t < triCount; ++t) {
                    for (int c = 0; c < 3; ++c) {
                        int vi = triV[t][c];
                        int vIdx = face.v[vi];
                        batch.positions.push_back(mesh.vx[vIdx]);
                        batch.positions.push_back(mesh.vy[vIdx]);
                        batch.positions.push_back(mesh.vz[vIdx]);
                        batch.texcoords.push_back(face.uv[vi][0]);
                        batch.texcoords.push_back(face.uv[vi][1]);
                        batch.vertexCount++;
                    }
                }
            }
        }
    }

    chunk.batches.clear();
    chunk.batches.reserve(batchMap.size());
    for (auto& [key, batch] : batchMap) {
        chunk.batches.push_back(std::move(batch));
    }
}

// ---------------------------------------------------------------------------
// IPDParse::ParseGlbFile
// Parse a standalone _GLB.PLM binary without a parent IPD.
// Objects are placed at their local-space origin (identity world transform).
// ---------------------------------------------------------------------------
bool IPDParse::ParseGlbFile(const std::string&          glbPath,
                              std::vector<RenderObject>&  outObjects,
                              std::vector<std::string>&   outTexNames,
                              std::vector<GlbObjectInfo>& outInfo)
{
    FILE* f = fopen(glbPath.c_str(), "rb");
    if (!f) {
        printf("[IPDParse::ParseGlbFile] Cannot open: %s\n", glbPath.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf((size_t)fileSize);
    if (fread(buf.data(), 1, fileSize, f) != (size_t)fileSize) {
        fclose(f);
        printf("[IPDParse::ParseGlbFile] Read error: %s\n", glbPath.c_str());
        return false;
    }
    fclose(f);

    if (buf.size() < sizeof(PLM_FILE_HEADER)) {
        printf("[IPDParse::ParseGlbFile] File too small: %s\n", glbPath.c_str());
        return false;
    }

    const PLM_FILE_HEADER* hdr = (const PLM_FILE_HEADER*)buf.data();
    if (hdr->id != 0x0630) {
        printf("[IPDParse::ParseGlbFile] Bad PLM magic 0x%04X: %s\n", hdr->id, glbPath.c_str());
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

        GlbObjectInfo info;
        info.name       = obj.name;
        info.mesh_id    = i;
        info.pack_count = packCount;

        outObjects.push_back(std::move(obj));
        outInfo.push_back(info);
    }

    printf("[IPDParse::ParseGlbFile] Loaded %d objects, %d textures from %s\n",
           (int)outObjects.size(), (int)outTexNames.size(), glbPath.c_str());
    return !outObjects.empty();
}
