# Open Hypotheses

Items here are provisional interpretations awaiting confirmation or disproof. Once resolved, they should be moved to `FACTS.md` (if confirmed) or deleted (if disproven).

---


## PLM Format Unknowns

- **`PLM_DATA_HEADER.num_c` — logical vs. physical normals?** `num_d` is confirmed as the physical normal array size (indices 0 to num_d−1). `num_c` (labelled "size = num_c × 4") is a smaller count that appears to correspond to the first logical cluster of normals (e.g. num_c=12, num_d=74 in THR0000 object THR1702 mesh 0). Hypothesis: `num_c` may count "base" or "smooth" normals, while entries num_c to num_d−1 may be per-face or derived normals. Requires cross-referencing with game rendering code.

## Map Overlay Unknowns

- **`field_38` in `s_MapOverlayHdr` — anim playback state snapshots?** The struct at this pointer (`s_UnkStruct3_Mo`, 8 bytes each) stores packed anim status flags, a fixed-point time value, and a keyframe index. Hypothesis: these define map-specific animation segment boundaries or transition states for Harry's custom anims, complementing `harryMapAnimInfos` at `0x34`. The two fields together may form a system: `0x34` describes *which* anim plays; `0x38` describes *how* the playback state machine transitions between keyframe ranges.

- **`field_5C`, `field_7C`, `field_94` in `s_MapOverlayHdr`:** Partially opaque nested structs. `field_5C` (40 bytes) appears on all maps. `field_7C` (32 bytes) only on `map1_s01` and `map6_s04`. `field_94` (~120 bytes, water particle emitter) only on `map1_s02` and `map1_s03`.

- **`unkTable1_4C` in `s_MapOverlayHdr`:** Array of 20-byte structs containing displacement offsets, angles, and timers. Likely enemy attack displacement / pushback data. Count given by `unkTable1Count_50`.

- **Secondary spatial grid (`sharedData_800DF2DC_0_s00`):** Full behavior of the secondary street grid used in outdoor maps (`MAP_HAS_SECONDARY_GRID` for `map0_s00`, `map0_s01`, `map2_s00`, `map2_s03`) is not yet documented.

*(Hypotheses are provisional; they will be removed once confirmed or disproven.)*