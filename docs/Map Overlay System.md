# Map Overlay System

This document describes how Silent Hill 1's map overlay system works — the "director" layer that controls all per-map logic, camera behavior, event triggers, enemy spawns, audio, and lighting. This is distinct from the streamed asset files (`.IPD`, `.PLM`, `.TIM`) which hold geometry and textures.

For binary struct layouts of the two primary data structs, see [`formats/Binary Overlays.md`](formats/Binary%20Overlays.md). For the complete research notes and struct-by-struct breakdown, see `tasks/BinaryOverlays/notes.md`.

---

## 1. Architecture Overview

Each map section in Silent Hill 1 has a **binary overlay** — a compiled PS1 executable overlay (`VIN/MAP*_S**.BIN`) that is loaded into a fixed RAM address when the player enters that area. There are **43 map overlays** total, indexed by `e_MapIdx`.

In the PC port, each overlay is compiled into a native shared library (`.dll` on Windows, `.so` on Linux) from C source files located in `src/maps/mapX_sYY/`. The game loads them dynamically via exported header symbols (`g_MapOverlayHeader_<name>`).

The overlay contains no collision geometry (that lives in `.IPD` chunks) and no visual geometry (`.PLM`/`.TIM`). Instead, it contains all per-map **logic and layout data** as static C-struct arrays.

**Master struct**: `s_MapOverlayHdr` (4172 bytes), defined in `include/bodyprog/map/map.h`.

---

## 2. Overlay Components

### Camera Paths (`vc_road_data.h`)
- **Struct**: `VC_ROAD_DATA` (24 bytes), capacity `CAMERA_PATH_COUNT_MAX = 100`
- **Termination**: Sentinel-flagged entry (`VC_RD_END_DATA_F`)
- **Contents**: Switch volume AABB (`lim_sw`), camera rail AABB (`lim_rd`), movement type (chase, fixed, self-view), height limits, look-at offset, lens flare type, fixed pitch/yaw angles
- **Coordinates**: Q4 (16 = 1 world unit)

### Waypoints (`map_points.h`)
- **Struct**: `s_MapPoint2d` (12 bytes), variable count, referenced by event triggers
- **Contents**: World XZ position (Q19.12), paper map thumbnail index, loading screen ID, trigger shape parameters
- **Role**: Defines spatial coordinates for door thresholds, examination points, and event trigger positions

### Event Triggers (`mapEvents` / `mapEventFuncs`)
- **Struct**: `s_EventData` (12 bytes), terminated by `TriggerType_EndOfArray`
- **Contents**: Trigger type and activation mode, waypoint index, required item, event flags, destination map, system state, SFX
- **Trigger types**: `TouchAabb`, `TouchFacing` (examine), `TouchObbFacing`, `TouchObb`
- **Role**: References waypoints by index; several events can share one waypoint

### Step-Height Collision Triggers (`header_field_D2C.h`)
- **Struct**: `s_CollisionTrigger` (4 bytes, bitfield-packed), capacity `COLLISION_TRIGGER_COUNT_MAX = 200`
- **Termination**: `isEndOfArray : 1` sentinel bit
- **Contents**: World XZ origin (10-bit signed), size (4-bit), height in half-metre steps (3-bit, range 0–3.5m)
- **Role**: Supplements IPD collision with discrete elevation snapping for stairs, kerbs, and ledges
- **Runtime**: Up to 20 nearby triggers are cached per frame via a ±16m sensing radius

### Enemy & NPC Spawns (`chara_spawns.h`)
- **Struct**: `s_SpawnInfo` (12 bytes), `charaSpawnInfos[2][16]` — two banks of 16 slots
- **Contents**: Character ID (`e_CharaId`), world XZ position (Q19.12), rotation (Q0.8), difficulty filter, spawn flags
- **Role**: `Game_NpcRoomInitSpawn` reads directly from the overlay; if a spawn isn't here, it doesn't exist for that map

### World Object Poses (`g_CommonWorldObjectPoses`)
- **Struct**: `s_WorldObjectPose` (20 bytes) — `VECTOR3` position + `SVECTOR3` rotation
- **Location**: In the overlay's data segment (not in `s_MapOverlayHdr` itself, but compiled into the same DLL)
- **Role**: Exact world position and rotation of every pickup item (ammo, health drinks, keys)

### Loadable Inventory Items (`LOADABLE_INVENTORY_ITEMS`)
- **Format**: Null-terminated `u8[]` of `e_InvItemId` values
- **Role**: Controls which item 3D models (`.TMD` packs) are loaded for inventory rendering on this map

### Scalars & Audio
- `bgmIdx` (s8) — BGM track index, indexes `g_BgmTaskLoadCmds[42]`
- `ambientAudioIdx` (u8) — Ambient VAB bank index, indexes `g_AmbientVabTaskLoadCmds[40]`
- `field_16` (s8) — Ambient tint: `0` = standard, `2` = otherworld transition, `3` = night/flashlight mode
- `field_17` (s8) — Weather: `0` = clear, `1` = rain, `2` = heavy rain, `3` = snow

---

## 3. Spatial Room System

The overlay defines a spatial grid that maps world coordinates to logical "room" IDs:

```
Player World Pos (X, Z) → Map_RoomIdxGet() → Grid Lookup (MAP_ROOM_IDXS) → Room ID (u8)
```

- **Function**: `Map_RoomIdxGet(q19_12 posX, q19_12 posZ)` — called every frame
- **Grid**: `MAP_ROOM_IDXS` maps 2D world space (divided by chunk cell size) to a 1-byte room ID
- **Purpose**: Controls BGM layer volume caps, ambient sound triggers, lighting/weather tints, and event scoping

There is no first-class "room" object in the engine. "Rooms" are a logical construct created by the combination of the spatial grid, door transitions, and audio context.

---

## 4. IPD Chunk Streaming Architecture

The overlay does **not** contain a hardcoded list of IPD chunk files. The connection is dynamic:

1. The overlay's `mapInfo` points to a `MAP_INFOS[e_MapType]` struct defining a 3-character `mapTag` (e.g. `"THR"`, `"ER"`)
2. During map load, `Map_MakeIpdGrid` scans the global file table for all `FileType_Ipd` files whose filenames begin with that `mapTag`
3. It parses the remaining 4 hex characters as X/Z grid coordinates (two's complement)
4. It slots each file index into the spatial lookup grid (`s_MapTerrain.chunkGrid`)
5. At runtime, the engine streams chunks into a 4-slot LRU cache (`activeChunks[4]`) based on player position

Different map overlays sharing the same `e_MapType` (e.g. `map0_s00` and `map0_s01` both using `MapType_THR`) share the exact same pool of IPD chunks. When transitioning to a different area type — such as entering the school bus interior from the streets — an event trigger fires a complete map reload, switching from `MapType_THR` to `MapType_ER`.

---

## 5. Dual Collision Systems

IPD collision and overlay collision triggers are **two separate, complementary systems**:

| | IPD Collision (`.IPD`) | Overlay Collision (`s_CollisionTrigger`) |
| :--- | :--- | :--- |
| **Storage** | Embedded in `.IPD` file header (`s_IpdCollisionData`) | Overlay header (`collisionTriggers` array) |
| **Geometry** | 2D subcell grid (20×20) with sloped planes & wall extrusions | Axis-Aligned Bounding Boxes |
| **Physics** | Plane equation via `baseGroundHeight` + gradients | Direct AABB point-in-box test |
| **Runtime role** | Continuous ground height, wall sliding, footstep SFX | Discrete step-height snapping (stairs, kerbs) |
| **Editor display** | Polygon mesh + extruded wireframe walls | Translucent 3D AABB boxes |

---

## 6. Custom Map Authoring Pathway

Creating a new custom map requires two pillars:

### Pillar 1: Streamed Asset Files
- `.IPD` — Chunk geometry + 2.5D collision heightfield
- `.PLM` — 3D prop models
- `.TIM` — VRAM textures and CLUT palettes
- File allocation table must be repacked if adding new filenames

### Pillar 2: Map Overlay (C Source → DLL)
- Author `SH_MAP_OVERLAY_HEADER` in C
- Populate camera paths, spawns, events, collision triggers, spatial room grid
- Compile via `add_map_overlay()` in CMake → `mapX_sYY.dll`
- Place DLL in the PC port's `maps/` directory

For the complete file layout and checklist, see `tasks/BinaryOverlays/2026-08-06_NEW_LEVEL_FILE_AND_ELEMENT_SPECIFICATION.md`.

---

## 7. Source File Locations

| Component | C Type | File Location (in decomp) |
| :--- | :--- | :--- |
| Master overlay header | `s_MapOverlayHdr` | `include/bodyprog/map/map.h` |
| Per-map overlay definitions | `SH_MAP_OVERLAY_HEADER = { ... }` | `src/maps/mapX_sYY/mapX_sYY_header.c` |
| Map build system | `add_map_overlay()` | `pc_port/maps/CMakeLists.txt` |
| Room index resolver | `Map_RoomIdxGet()` | `include/maps/shared/Map_RoomIdxGet.h` |
| Extracted overlay data | Static arrays from PS1 `.BIN` | `pc_port/build_gen/extracted_data/` |
| Extraction tool | `extract_map_data.py` | `pc_port/tools/extract_map_data.py` |
