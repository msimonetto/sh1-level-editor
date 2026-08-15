# Task: Room Linkage Editor

**Parent task:** `tasks/unified_cpp_editor`  
**Status:** Planning → Active  
**Phase:** Phase 3 (Door Links & Waypoint Editing) per `PLANNED_FEATURES.md`

---

## Objective

Implement an interactive viewport tab and inspector panel for viewing and editing the
door waypoints (`s_MapPoint2d`) and event triggers (`s_EventData`) that link map
sections together in Silent Hill 1.

### Capabilities (in scope)
- Visualise waypoint arrival points and precise trigger volumes in the synced 3D viewport
- Edit waypoint positions (drag in 3D), arrival angles, paper map index, loading screen ID
- Inspect and edit all `s_EventData` fields (trigger type, activation, sysState, flags)
- Reassign which `s_MapPoint2d` a door trigger targets
- Change the destination map (`mapIdx`, 0-42) including maps not in the workspace
- Add or delete waypoints and event links (CRUD)
- **Link** (one-way) and **Link Bidirectionally** (mutates both overlay JSONs)
- Serialize edits back to `data/workspace/overlays/<MAP_KEY>/map_points.json` and `events.json`

### Out of scope (deferred)
- Direct BIN binary editing (deferred to a future sub-task of `unified_cpp_editor`)
- Author mode / creating overlays from scratch (deferred)
- DLL recompilation / propagation

---

## Prerequisite

`data/workspace/overlays/` must be populated by `tooling/scripts/extract_overlay.py`
before the C++ editor can load any real data. Write and test the Python script first.

---

## Key Data Structures (from decomp source)

Trigger geometry is **entirely embedded in `s_MapPoint2d`** (verified in `events_main.c`):

| `triggerType` | Shape | Params |
|---|---|---|
| `TouchAabb` | AABB | Half-extents: X = `param0 x 0.25m`, Z = `param1 x 0.25m` |
| `TouchFacing` | Circle (r=0.8m) + +-30 degree cone | No size params |
| `TouchObbFacing` | Rotated rect + facing | `param0` = angle (Q8), `param1` = half-length |
| `TouchObb` | Rotated rect | Same, width fixed at 4.0m |

Event colour coding (by `sysState`):
- Green = `LoadRoom`/`LoadOverlay` (active door)
- Amber = `ReadMessage` with locked-door `eventParam` (11-13)
- Grey = scripted non-navigational
- Blue = save point
- Orange label = cross-map transition (different `mapIdx` prefix)

---

## Files

### New files
- `tooling/scripts/extract_overlay.py` -- Python extractor (phase A)
- `include/core/OverlayData.h` -- editor-friendly (no bitfields) structs
- `include/core/OverlayLoader.h` / `src/core/OverlayLoader.cpp` -- JSON I/O
- `include/viewport/EventViewport.h` / `src/viewport/EventViewport.cpp` -- 3D viewport
- `include/core/LinkageInspector.h` / `src/core/LinkageInspector.cpp` -- ImGui inspector

### Modified files
- `include/core/ipd_structs.h` -- add `MapPoint2d` + `EventData` packed structs
- `include/core/ViewportSync.h` / `src/core/ViewportSync.cpp` -- add `EventViewport` sync
- `src/main.cpp` -- wire in `EventViewport` + `LinkageInspector`
- `tasks/unified_cpp_editor/ARCHITECTURE.md` -- add new entries

---

## Phase Breakdown

| Phase | Deliverable |
|---|---|
| A | `extract_overlay.py` -- populates `data/workspace/overlays/` |
| B | `OverlayData.h` + `OverlayLoader.cpp` -- JSON load/save |
| C | `EventViewport.cpp` read-only -- coloured pins + trigger volumes |
| D | Waypoint CRUD + `LinkageInspector` panel |
| E | Link / Link Bidirectionally CRUD, all 43 maps in dropdown |
| F | `OverlayLoader::Save()` round-trip write-back |