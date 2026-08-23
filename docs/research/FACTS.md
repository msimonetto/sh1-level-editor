# Confirmed Facts

All entries below are backed by decompiled source code (PC port decomp), reproducible byte-exact round-trip tests, or cross-validation across multiple sample files. Items are grouped by system.

---

## IPD File Format

- **File type code:** In the game's format table, `.IPD` is type code 6, denoting map/world geometry data.
- **Endianness:** All integer and float values in IPD files use little-endian byte order (PS1 MIPS R3000 CPU is LE), confirmed by matching test values to known coordinates via `sh_ipd2obj` source code.
- **`s_IpdHeader` size:** 84 bytes (`0x54`). Supported by `STATIC_ASSERT_SIZEOF(s_IpdHeader, 84)` in `ipd.h`. *(Note: These sizes apply to the 32-bit PSX on-disk layout used by modders. On 64-bit PC builds, `ipd_reformat.c` dynamically expands these structs in memory to accommodate 64-bit pointers.)*
- **`s_IpdModelBuffer` size:** 24 bytes. Supported by `STATIC_ASSERT_SIZEOF(s_IpdModelBuffer, 24)` in `ipd.h`. (Previously referred to as `IPD_POS_HEADER`).
- **`s_IpdModelInstance` size:** 36 bytes. Supported by `STATIC_ASSERT_SIZEOF(s_IpdModelInstance, 36)` in `ipd.h`. (Previously referred to as `IPD_OBJ_DATA`).
- **`s_IpdModelInfo` size:** 16 bytes. Supported by `STATIC_ASSERT_SIZEOF(s_IpdModelInfo, 16)` in `ipd.h`. (Previously referred to as `IPD_OBJNAME_DATA`).
- **Model buffer stride:** `modelBuffers(i) = base + 24 * i`.
- **Model instance stride:** `modelInstances(j) = base + 36 * j`.
- **Model info table offset (`modelInfo`):** Dynamically calculated based on section counts.
- **Model buffer offset (`modelBuffers`):** Dynamic.
- **`s_IpdModelInstance.mat` padding field:** Always `0x0000` across all parsed instances. Natural C struct alignment padding for the `MATRIX`.
- **Global object flag:** In `s_IpdModelInfo`, `isGlobalPlm=0` means the mesh lives in the IPD's embedded PLM section; `isGlobalPlm=1` means it lives in the separate `_GLB.PLM` file.
- **`minX`/`maxX`/`minZ`/`maxZ` in `s_IpdModelBuffer` are NOT zero:** These are bounding box extents (Q7.8 fixed point), previously referred to as `unk2` / `unk3`.
- **`field_10` and `subcellPositions` blocks:** Each `s_IpdModelBuffer` contains pointers at 0x10 and 0x14 pointing to array data (XZ positions and unknown collision data).
- **`modelOrderList` is non-zero:** Points to a valid dynamic address for the object ordering array (previously known as `unkdata_offset`).
- **All struct sizes:** All 9 IPD/PLM structs pass `struct.calcsize` self-test against hand-derived totals from `main.c`.
- **Round-trip:** All 5 sample IPD files (THR0000–THR0004) pass byte-identical decode/re-encode using observed structs + opaque gap blobs. No byte is lost.

## IPD Naming Convention

- **Filename structure:** Every IPD file matches `^([A-Z]+)([0-9A-F]{2})([0-9A-F]{2})\.IPD$` — a 2–3 letter map prefix followed by two hex-encoded coordinates (X, Z).
- **Coordinate encoding:** The two hex digits are 8-bit signed two's complement integers: `00`–`7F` → 0 to 127, `80`–`FF` → −128 to −1.
- **Confirmed by decomp:** `Map_MakeIpdGrid` in the PC port scans the global file table for all `FileType_Ipd` files whose filenames begin with the active map's `mapTag` (e.g. `"THR"`), parses the remaining 4 characters as hex X/Z coordinates, and slots the file index into a spatial lookup grid. This confirms the naming convention is engine-enforced, not just a convention.

## PLM Format

- **Texture name table:** The IPD's embedded PLM section contains a texture-name table (list of 24-byte null-padded strings). Entry count is given by `PLM_FILE_HEADER.tex_num`, observed via `sh_ipd2obj`.
- **`PLM_DATA_HEADER.num_d` is the normal array entry count:** `num_d` determines the actual number of accessible normal entries (indices 0 to num_d−1). Pack normal indices (`normals_0..normals_3`) can reference up to index num_d−1. Evidence: byte-exact JSON round-trip across THR0000/THR0001/SU0000/SU0001/SPU0000 confirms that storing `num_d` entries reproduces output identically.
- **STP/`unk2` flag in `PLM_PACK_HEADER`:** Bit 7 of the packed byte (`tex_num_and_unk2_byte`) is the `unk2` flag, corresponding to Semi-Transparency (STP) control. Passing this into alpha generation logic identically matches the original C executable's output.

## TIM Texture Format

- **TIM_FILE_HEADER** is exactly 8 bytes long. **TIM_CLUT_HEADER** is 12 bytes long. The CLUT palette data physically starts at offset 20.

## Collision System

- **Collision Header (`s_IpdCollisionData`):** The 308-byte struct starting at `0x0054` is `s_IpdCollisionData`. It contains pointers and counts defining several arrays (`splitVertices`, `surfaces`, `subcells`, `ptr_18`, `subcellRanges`, `ptr_28`, `ptr_2C`, `subcellCheckIdx`).
- **Collision Spatial Grid (`subcellRanges`):** The `subcellRanges` pointer (offset `0x20` into the collision header) points to an array of `s_IpdCollSubcellRange` elements (4 bytes each). This represents the map's spatial partitioning, dimensioned by `subcellCountX` and `subcellCountZ`. (This explicitly refutes previous assumptions of a hardcoded 20×20 broadphase grid).
- **Collision Subcells (`s_IpdCollSubcell`):** Exactly 10 bytes. The first 6 bytes pack X, Y, and Z coordinates along with ID flags, followed by 4 `u8` indices (`splitVertexIdx0`, `splitVertexIdx1`, `surfaceIdx0`, `surfaceIdx1`) into the `splitVertices` and `surfaces` arrays.
- **Collision Walls vs Floors:** The 2.5D grid does not construct 3D boxes. Walls are rendered by extruding the 2D line segment between `splitVertexIdx0` and `splitVertexIdx1` vertically on the Y-axis when the surface is impassable (`0xFF` or `disableHeight = true`). Floors are derived from the broadphase grid and the `baseGroundHeight` of the active surface.
- **Dual collision systems:** IPD collision (2.5D heightfield in `.IPD` chunks) and overlay collision triggers (`s_CollisionTrigger` in the map overlay) are two completely separate, complementary systems. IPD handles continuous sloped planes and wall extrusions. Overlay triggers handle discrete step-height snapping for stairs, kerbs, and ledges via AABBs.

## Pipeline & Tooling

- **JSON Intermediate Losslessness:** Storing all fixed-point geometry integers and opaque gaps as raw integers/hex strings in JSON preserves 100% of the information needed to reproduce byte-identical OBJ/MTL/TGA output across all tested map prefixes (THR, SU, SPU). No floating-point precision loss occurs when intermediate values stay as integers.
- **PC Port Loose File System:** The PC port supports a loose-file override (`allow_loose_files=1`) that reads files from `gamedata/load/<FOLDER>/` instead of the disc image. This is the standard approach for asset modding and testing. Earlier extraction tools (e.g. Vatuu's `extract.py`) truncated files at the file-table's 256-byte block boundary, discarding sector padding that contained valid data — this caused crashes when zero-padding couldn't substitute for the missing struct data. Modern extraction (SHExtract from `.bin`/`.cue`) or the loose-file override avoids this entirely.

*(Facts are subject to revision if future evidence contradicts them.)*