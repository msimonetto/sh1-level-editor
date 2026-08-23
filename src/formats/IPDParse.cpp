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

#include "formats/IPDParse.h"
#include "formats/Structs.h"
#include "formats/PLMParse.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <map>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

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
        // Try workspace/PLM/{PREFIX}_GLB.PLM
        std::string glbPath = workspaceDir + "/PLM/" + out.chunkPrefix + "_GLB.PLM";
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
                if (PLMParse::ParseAndPlaceObject(srcBuf.data(), srcBuf.size(),
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
