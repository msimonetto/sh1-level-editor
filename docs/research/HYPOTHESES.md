# Current Hypotheses

> [!WARNING]
> This documentation is outdated and needs to be updated.

- **`unk2` / `unk3` in `IPD_POS_HEADER` — draw distance or bounding info?:** Binary dump shows these two `int` fields contain non-trivial data (e.g. `0x07FF0000`, `0x0FFF0800`). Pattern observation: `unk2` appears to increase monotonically between position groups while `unk3` repeats `0x07FF0000` in several groups. Both look like packed 16-bit pairs. Hypothesis: these may encode draw-distance near/far limits or bounding box extents per object group. Needs cross-reference with game rendering code.

- **`unk2_offset` / `unk2_num` in `IPD_POS_HEADER` — 8-byte blocks:** Each group has an `unk2_offset` pointing to an 8-byte block (confirmed to exist). Source comment: "unknown2 data offset, 8 byte per pack?". Hypothesis: these 8-byte blocks may each contain a bounding sphere or view-distance anchor position (one 3D vertex = 6 bytes + 2 bytes alignment, or 2×int32). Needs content inspection (request patched tool's GAP2.BIN output or direct hex inspection).

- **`unk1_data[52]` in `IPD_FILE_HEADER` — drawing distance global table?:** The 52-byte field contains non-zero data in THR0000 (e.g. `00 04 04 06 0a 06 10 06 ...`). Source comment: "drawing distance global table?". The values look like pairs of small integers (possibly object-ID to draw-distance mappings, one byte each). Requires comparison across multiple IPD files to find the pattern. `unk1_num = 129` may relate to the count of entries in this table.

- **`unkdata_offset` in `IPD_FILE_HEADER` — present and non-zero:** Confirmed `0x09D4` in THR0000. Source comment: "looks like obj indices (pos from the list); 1-byte * unk1_num; drawing distance table per obj?". With `unk1_num = 129`, this would suggest a 129-byte table of object indices at 0x09D4. This falls inside the large gap `[0x0480, 0x1694)`. Needs hex inspection of bytes at 0x09D4.

- **`IPD_POS_HEADER.pad` field (the `short pad` in `IPD_OBJ_DATA`):** Binary dump confirms `pad = 0x0000` for all instances inspected so far. Hypothesis: always zero. Needs verification across all 33 instances and other IPD files.

- **Object ordering:** No confirmed pattern yet for the ordering of position groups. The hypothesis that they are sorted by Y position (for rendering order) remains unverified.

- **IPD Naming and Map Grid:** Files like `THR0000.IPD` have headers where `x_pos=0` and `y_pos=0`. Hypothesis: The digits in the filename directly correspond to these grid coordinates (e.g., `XXYY`), and the game engine uses this strict naming/grid convention to cluster and stream map chunks into RAM without overlapping.

- **`PLM_DATA_HEADER.num_c` — logical vs. physical normals?:** `num_d` is confirmed as the physical array size (0..num_d-1 are accessible). `num_c` (labelled "size = num_c * 4") is a smaller count that appears to correspond to the first logical cluster of normals (e.g. num_c=12, num_d=74 in THR0000 object THR1702 mesh 0). Hypothesis: `num_c` may be the count of "base" or "smooth" normals, while entries num_c..num_d-1 may be per-face or derived normals. This requires cross-referencing with game rendering code.

*(Hypotheses are provisional; they will be removed from this file once confirmed or disproven.)*