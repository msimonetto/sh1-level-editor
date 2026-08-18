# Confirmed Facts

All entries below are backed by decompiled source code (PC port decomp), reproducible byte-exact round-trip tests, or cross-validation across multiple sample files. Items are grouped by system.

---

## IPD File Format

- **File type code:** In the game's format table, `.IPD` is type code 6, denoting map/world geometry data.
- **Endianness:** All integer and float values in IPD files use little-endian byte order (PS1 MIPS R3000 CPU is LE), confirmed by matching test values to known coordinates via `sh_ipd2obj` source code.
- **IPD_FILE_HEADER size:** 84 bytes (`0x54`). Supported by `struct.calcsize` matching the hand-summed field sizes and the decomp's own comment: "all offsets += sizeof(IPD_FILE_HEADER) (0x54)".
- **IPD_POS_HEADER size:** 24 bytes. Supported by stride arithmetic across all groups.
- **IPD_OBJ_DATA size:** 36 bytes. Supported by data-offset deltas across all instances.
- **IPD_OBJNAME_DATA size:** 16 bytes. Supported by `16 * dta.obj_id` indexing in `main.c` and binary layout.
- **Position header stride:** `hpos(i) = obj_data_offset + 24 * i`, observed for all 19 groups in THR0000.IPD with zero deviations.
- **Object instance stride:** `dpos(j) = data_offset + 36 * j`, observed for all instances across all 19 groups (dpos deltas = 36 bytes, zero deviations).
- **Object name table offset (`obj_name_offset`):** Dynamically calculated based on section counts. In `THR0000.IPD`, `obj_name_offset = 0x0188` (392 decimal), containing 19 entries of 16 bytes each.
- **Object data offset (`obj_data_offset`):** Dynamic. In `THR0000.IPD`, observed at `0x02B8` (696 decimal), verified by both stride arithmetic and binary dump.
- **`IPD_OBJ_DATA.pad` field:** Always `0x0000` across all parsed instances and all tested IPD files. Natural C struct alignment padding.
- **Global object flag:** In `IPD_OBJNAME_DATA`, `flag=0` means the mesh lives in the IPD's embedded PLM section; `flag=1` means it lives in the separate `_GLB.PLM` file. Matches logic in `main.c` and binary dumps.
- **`unk2` / `unk3` in IPD_POS_HEADER are NOT zero:** Binary dump shows non-zero values (e.g. `0x07FF0000`, `0x0FFF0800`). These fields carry real data and should not be treated as padding.
- **8-byte sub-blocks between OBJ_DATA arrays:** Each `IPD_POS_HEADER.unk2_offset` (and `unk1_offset` when `unk1_num > 0`) points to an 8-byte block immediately following the corresponding `IPD_OBJ_DATA` array. When `unk1_num = 0`, `unk1_offset == unk2_offset` (same pointer). Confirmed by gap analysis and round-trip tests.
- **`unkdata_offset` is non-zero:** Points to a valid dynamic address (e.g. `0x09D4` in `THR0000.IPD`), falling inside the data region between the position array and the PLM section. Content appears unread by `sh_ipd2obj`.
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

- **Collision Header (`s_IpdCollisionData`):** The 308-byte region starting at `0x0054` is the `s_IpdCollisionData` struct. It contains internal pointers and counts defining 7 separate arrays (splitVertices, surfaces, subcells, unkBlock3, broadphase grid, block5, block6).
- **Collision Broadphase Grid:** The `grid` pointer (offset `0x20` into the collision header) points to an array of `s_IpdCollSubcellRange` elements (4 bytes each). This represents the map's spatial partitioning (often 20×20, given by `gridWidth × gridHeight`). These ranges point into `ptr_block5`, not `subcellCheckIdx`. The `subcellCheckIdx` buffer (embedded at `0x34` / 0x88) is a completely separate per-frame dedup counter array.
- **Collision Subcells (`s_IpdCollSubcell`):** Exactly 10 bytes: 3 packed `short` values followed by 4 `u8` values. Byte layout confirmed by round-trip.
- **Collision Subcell Meaning (RESOLVED):** `field_0` packs an X coordinate (lower 14 bits, sign-extended). `field_2` packs a Y height (lower 14 bits, sign-extended). `field_4` is a raw 16-bit Z coordinate. The trailing 4 bytes are flat `u8` indices (`splitVertexIdx0`, `splitVertexIdx1`, `surfaceIdx0`, `surfaceIdx1`) into the `splitVertices` and `surfaces` arrays. `0xFF` acts as a NULL/impassable surface flag.
- **Collision Walls vs Floors:** The 2.5D grid does not construct 3D boxes. Walls are rendered by extruding the 2D line segment between `splitVertexIdx0` and `splitVertexIdx1` vertically on the Y-axis when the surface is impassable (`0xFF` or `disableHeight = true`). Floors are derived from the broadphase grid and the `baseGroundHeight` of the active surface.
- **Dual collision systems:** IPD collision (2.5D heightfield in `.IPD` chunks) and overlay collision triggers (`s_CollisionTrigger` in the map overlay) are two completely separate, complementary systems. IPD handles continuous sloped planes and wall extrusions. Overlay triggers handle discrete step-height snapping for stairs, kerbs, and ledges via AABBs.

## Pipeline & Tooling

- **JSON Intermediate Losslessness:** Storing all fixed-point geometry integers and opaque gaps as raw integers/hex strings in JSON preserves 100% of the information needed to reproduce byte-identical OBJ/MTL/TGA output across all tested map prefixes (THR, SU, SPU). No floating-point precision loss occurs when intermediate values stay as integers.
- **PC Port Loose File System:** The PC port supports a loose-file override (`allow_loose_files=1`) that reads files from `gamedata/load/<FOLDER>/` instead of the disc image. This is the standard approach for asset modding and testing. Earlier extraction tools (e.g. Vatuu's `extract.py`) truncated files at the file-table's 256-byte block boundary, discarding sector padding that contained valid data — this caused crashes when zero-padding couldn't substitute for the missing struct data. Modern extraction (SHExtract from `.bin`/`.cue`) or the loose-file override avoids this entirely.

*(Facts are subject to revision if future evidence contradicts them.)*