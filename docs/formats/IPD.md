# IPD Binary Format Reference

Structures and layouts for `.IPD`, `.PLM`, and `.TIM` files. Entries are marked **[Confirmed]** (byte-exact roundtrip or decompiled source) or **[Provisional]** (consistent with observed data, not yet code-verified).

For collision data, see [`Collision.md`](Collision.md). For intermediate JSON/PNG formats, see [`JSON.md`](JSON.md).

## IPD (.IPD) Map Format

### Purpose
Holds level geometry and object placement data for a game map stage. Includes vertices, polygons, texture references, and map object records.

### FACTS: Verified Binary Structures

The following structures and layouts have been definitively confirmed by our Python parser (`ipd_structs.py`) and matched perfectly against the original `sh_ipd2obj` output. All multi-byte fields are **Little-Endian**.

#### 1. IPD_FILE_HEADER (84 bytes / 0x54)
The root header of the IPD file.
- `id` (1 byte, uint8): Magic identifier; confirmed to be `0x14` for valid `.IPD` files.
- `x_pos`, `y_pos` (1 byte each, int8): Map coordinates (often matches the IPD filename, e.g., 3 and 1 for ER0301).
- `plm_offset` (4 bytes, uint32): Absolute byte offset to the embedded `PLM_DATA_HEADER` section (containing mesh/texture data).
- `pos_num` (4 bytes, uint32): Number of `IPD_POS_HEADER` layout clusters.
- `obj_name_offset` (4 bytes, uint32): Absolute offset to the string table for object IDs.
- `obj_data_offset` (4 bytes, uint32): Absolute offset to the layout geometry hierarchies (`IPD_POS_HEADER`).

#### 2. IPD_POS_HEADER (24 bytes / 0x18) [Confirmed]
Acts as a grouping node for a collection of objects. Stride: `hpos(i) = obj_data_offset + 24 * i`.
- `obj_num` (2 bytes, uint16): Number of `IPD_OBJ_DATA` structs in this group.
- `data_offset` (4 bytes, uint32): Offset to this group's `IPD_OBJ_DATA` array.
- `unk1_offset`, `unk1_num` (4 bytes each): Pointer and count for unknown block 1 (8-byte blocks). When `unk1_num = 0`, equals `unk2_offset`.
- `unk2_offset`, `unk2_num` (4 bytes each): Pointer and count for unknown block 2 (8-byte blocks, immediately follow `IPD_OBJ_DATA` array).
- `unk2`, `unk3` (4 bytes each): Non-zero. Possible packed 16-bit near/far draw-distance values. See `docs/HYPOTHESES.md`.

#### 3. IPD_OBJ_DATA (36 bytes / 0x24) [Confirmed]
Represents a physical object/mesh placed in the world. Stride: `dpos(j) = data_offset + 36 * j`.
- `rx, ry, rz` (2 bytes each, int16): Euler rotations.
- `pad` (2 bytes): Always `0x0000`. C natural-alignment padding.
- `tx, ty, tz` (4 bytes each, int32): World translations (fixed-point).
- `glb_flag` (4 bytes, int32): `0` = geometry in embedded PLM; `1` = geometry in `_GLB.PLM`.
- `mesh_id` (4 bytes, int32): Index into the PLM objects array.

#### 4. PLM_DATA_HEADER (16 bytes / 0x10)
Defines the mesh geometry arrays embedded within the IPD.
- `num_c` (4 bytes, uint32): Normal sub-count (purpose unclear — may delimit "base" vs "derived" normals; see `docs/research/HYPOTHESES.md`).
- `num_d` (4 bytes, uint32): **Normal array entry count.** Pack normal indices (`normals_0..normals_3`) reference entries 0 to num_d−1. This is the physical array bound, not `num_c`.
- `obj_offset` (4 bytes, int32): Offset to the start of the `PLM_OBJ_DATA` table.
- `normal_offset` (4 bytes, int32): Offset to vertex normals array. *Note: Normal arrays can occasionally terminate early; out-of-bound normal fetches default to (0,0,0) in game code.*

#### 5. PLM_PACK_HEADER (16 bytes / 0x10)
Defines a quad-face and its texturing data.
- `u0, v0`, `u1, v1`, `u2, v2`, `u3, v3` (1 byte each, uint8): Texture UV coordinates mapping directly to a VRAM-loaded TIM texture.
- `cba` (2 bytes, uint16): Encodes the Color Lookup Table (CLUT) for the texture. Bitwise shift operations extract precise palette coordinates in VRAM.
- `tex_num` (1 byte, 7 bits used): The index of the TIM texture string inside the PLM texture list.
- `faces` (4 bytes, uint8 array): Vertex indices forming the quad.

#### 6. TIM Texture Format
The game uses standard PlayStation `.TIM` files (15-bit RGB palette + 1 transparency bit). Texture assignments are made by reading an array of 8-character string tags at the end of the PLM block (e.g., `ER_INDI1`) which explicitly map to external `.TIM` files located in the same directory.
- **Conversion Mathematics**: Translating PS1 5-bit colour channels (0-31) to standard 8-bit PC channels (0-255) must be done with strictly invertible integer mathematics (`c * 255 // 31`) instead of float multiplication (e.g. `c * 8.226`) to avoid rounding corruption during byte-for-byte `.TIM` to `.TGA` backwards conversions.
- **Transparency (STP bit)**: Standard semi-transparency is dictated by the highest bit in the 16-bit word. A fully transparent pixel is strictly `0x0000`, while `0x8000` functions as semi-transparent black. Any colour with the STP bit set (`c & 0x8000`) is drawn as semi-transparent.

---

### Known Unknowns (Provisional / Hypothesis)

1. **`unk1_data` (52 bytes in `IPD_FILE_HEADER`):** Non-zero in THR0000 (`00 04 04 06 0a ...`). Source comment: "drawing distance global table?". May be pairs of object-ID-to-draw-distance mappings. Requires cross-map comparison.
2. **`unkdata_offset` in `IPD_FILE_HEADER`:** Confirmed non-zero (`0x09D4` in THR0000). Source comment: "1-byte × unk1_num drawing distance table per obj?". Falls in the gap between object data and PLM. Needs hex inspection.
3. **`unk2` / `unk3` in `IPD_POS_HEADER`:** Non-zero packed values (e.g. `0x07FF0000`, `0x0FFF0800`). Increase monotonically between position groups. Provisional hypothesis: packed near/far draw-distance limits or bounding box extents. See `docs/research/HYPOTHESES.md`.
4. **8-byte blocks at `unk2_offset`:** Confirmed to exist immediately after each `IPD_OBJ_DATA` array. Content unread by `sh_ipd2obj`. Possible bounding sphere (6 bytes XYZ + 2 bytes alignment).

### Relationships
- If an `IPD_OBJ_DATA` node has `glb_flag == 1`, the game loads geometry from a master level PLM (e.g., `THR_GLB.PLM` for `THR0000.IPD`). If missing, game tools crash or silently abort geometry generation for that ID.
- UVs map 1:1 to external `.TIM` files. The IPD determines the bounds (u,v) and palette (CLUT), but the TIM files hold the uncompressed indexed pixel data.

### Validation
- **JSON Round-Trip:** The JSON intermediate pipeline (`ipd-json` → `json-ipd`) produces byte-identical binary output across all tested map prefixes (THR, SU, SPU). OBJ export is a lossy visualization format and is not used for binary validation.
- **Normal Boundaries:** Safely bypasses segmentation faults found in the original tools when parsing incomplete/padded normal clusters.

### References
- SlickAmogus Silent Hill decomp (PC port) – shows file type codes and may contain code reading IPD.  
- sh_ipd2obj GitHub repo – for practical output of reading IPD.  
- Community documentation (Silent Hill Hub) – general guidance on file format research.

---

## Pipeline Implications for Novel IPD Authoring

When creating a *new* IPD from scratch (rather than round-tripping an existing one), several areas require dynamic calculation rather than static copying:

- **Collision payload** (`IPD_COLL_HEADER`): Defines subcells, surfaces, and split vertices embedded at offset `0x54`. See [`Collision.md`](Collision.md) for the full struct layout.
- **`unk2_offset` blocks**: Must be recalculated per object group after any geometry insertion. Their size is always exactly 8 bytes per group.
- **PLM alignment padding**: PLM sections must be padded to 4-byte boundaries dynamically based on the new geometry count, not by copying static `0x0000` gaps.
- **`obj_data_offset` and `obj_name_offset`**: Both are dynamic and must be recalculated when object counts change.