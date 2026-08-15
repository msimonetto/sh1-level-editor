# Task: Binary Overlay Analysis

## Goal

(Formerly) Fully parse and round-trip every per-map binary overlay (`VIN/MAP*_S**.BIN`) into a set of
structured JSON files — one JSON per logical component — and verify lossless reconstruction.
This is the map-level equivalent of the existing IPD/PLM/TIM chunk pipeline.

The overlay files are the "director" for each map section: they embed camera paths, event
triggers, spawn configs, audio bindings, collision step-zones, world object poses, and
item inventory lists — all as flat C-struct arrays compiled into a single PS1 executable
overlay. We have all 43 BIN files in `data/original/proper_extract/VIN/`.

See `notes.md` for a full breakdown of every component, its source struct, and its byte size.

## Status

**Architectural Analysis & Pathway Complete.** See `2026-08-06_OVERLAY_ARCHITECTURE_AND_CUSTOM_MAP_PATHWAY.md` for full documentation.
The task has been simplified from low-level binary blob parsing to high-level C struct authoring and dynamic library (`.dll`) integration.

## Next Steps

- [ ] Cross-check upstream repository updates to resolve remaining unknown struct schemas in `extracted_data.c` (cutscene audio commands, BGM tables, boss rodata).
- [ ] Incorporate overlay AABB step-height visualization into `unified_cpp_editor` as a dedicated viewport layer alongside IPD 2.5D heightfield geometry.
- [ ] Verify ROM file allocation table repacking workflow for registering custom `.IPD` and `.TIM` assets.

