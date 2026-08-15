# Overlay Architecture & Custom Map Pathway

**Date:** 2026-08-06  
**Status:** Architectural Analysis & Pathway Defined  
**Task:** `tasks/binary_overlay_analysis`  

---

## 1. Executive Summary & Paradigm Shift

Prior to this analysis, the `binary_overlay_analysis` task was focused on reverse-engineering and parsing raw PS1 binary overlay files (`VIN/MAP*_S**.BIN`) into structured JSON for round-trip binary re-packing. 

Analysis of the decompilation repository (`external/SlickAmogus_silent-hill-decomp`) and its PC port build system (`pc_port/maps/CMakeLists.txt`) reveals a significantly simpler, cleaner pathway:
1. **Source-Level Mapping:** The map overlays are C source files located in `src/maps/mapX_sYY/`.
2. **Dynamic Library Architecture:** In the PC port, each map overlay is compiled directly into a native shared library (`.dll` on Windows, `.so` on Linux).
3. **Symbol Export:** The game executable loads map overlays at runtime via dynamic link exports (`g_MapOverlayHeader_<map_name>`), matching PS1 RAM loading behavior without requiring binary blob byte-patching.

This shifts the goal from binary reverse-engineering to **high-level data structure authoring** for custom maps and map editing.

---

## 2. Comprehensive Pathway for Adding Custom Maps

To add a functional custom map or modify an existing map, the complete end-to-end pipeline requires:

```
[ Custom Geometry & Textures ]          [ Custom Logic & Overlay Header ]
    │                                          │
    ├── Export .IPD (Mesh + 2.5D Height)       ├── `SH_MAP_OVERLAY_HEADER` (C Struct)
    └── Export .TIM (Textures)                 ├── Camera Paths, Spawns, Events, Triggers
            │                                  └── Build via CMake -> mapX_sYY.dll
            ▼                                          │
[ Repack ROM File Allocation Table ]                  │
            │                                          │
            └───────────────► [ Game Engine Runtime ] ◄┘
```

### Steps:
1. **Geometry & Textures:**
   - Create and export the visual mesh and 2.5D heightfield physics into `.IPD` format.
   - Create and export textures into `.TIM` format.
2. **ROM File Table Repacking (CRITICAL):**
   - **Crucial Requirement:** Newly created `.IPD` and `.TIM` files cannot simply be placed in a directory; the game ROM's file allocation table / index must be repacked so the engine's file system driver recognizes the new file entries and sector offsets on disc (`ST/` folder).
3. **Map Overlay Authoring (C Source / DLL):**
   - Author a map overlay header (`SH_MAP_OVERLAY_HEADER` / `_MapOverlayHdr`) in C.
   - Populate the required event scripts, camera paths, spawn data, and spatial room grids.
   - Compile the map directory into a standalone `.dll` / `.so`.
4. **Runtime Loading:**
   - Place the generated `mapX_sYY.dll` into the PC port's `maps/` directory. The engine loads it dynamically when entering the room.

---

## 3. Collision Architecture Reconciliation: IPD vs. Binary Overlay

A potential ambiguity existed regarding whether IPD collision data (2.5D heightfield) contradicted the binary overlay's collision data (`s_CollisionTrigger`). These are **two separate, complementary systems** operating in tandem:

| Feature | IPD Collision System (`.IPD`) | Binary Overlay Collision System (`s_CollisionTrigger`) |
| :--- | :--- | :--- |
| **Storage Location** | Embedded inside `.IPD` file header (`s_IpdCollisionData`) | Embedded in overlay header (`.collisionTriggers` array in `header_field_D2C.h`) |
| **Geometry Model** | 2D top-down Subcell grid (20x20) with sloped planes & walls | Axis-Aligned Bounding Boxes (AABB) / Oriented Bounding Boxes (OBB) |
| **Physics Calculations** | Plane equation via `baseGroundHeight` + gradients (`tiltAngleX` pitch, `tiltAngleZ` roll). Wall extrusion via `disableHeight = true`. | Direct AABB/OBB point-in-box testing. |
| **Runtime Role** | Broad-phase collision: player raycasting, wall sliding, continuous slopes, footstep SFX types. | Step-height snapping: forces player $Y$ position to a fixed step height when inside the box. |
| **Use Case** | Streets, hills, ramps, continuous room floors, solid walls. | Sharp vertical transitions (stairs, kerbs, ledges) where continuous sloped planes cause model clipping. |

### Application to `unified_cpp_editor`
The `unified_cpp_editor`'s `CollisionViewport` accurately models and renders the IPD collision mesh (sloped polygon planes and extruded wall lines). To achieve complete collision fidelity, the editor should display **two separate viewport layers**:
1. **IPD Base Physics Layer:** Polygons and extruded wireframe walls from `.IPD`.
2. **Overlay Elevation Layer:** Translucent 3D AABB bounding boxes from `s_CollisionTrigger` overlay data.

---

## 4. Decomp & PC Port Architecture (DLLs & Extracted Data)

### Source File Mapping
In `external/SlickAmogus_silent-hill-decomp/src/maps/mapX_sYY/`, binary overlay contents are mapped out into C header arrays included by `mapX_sYY_header.c`:

*   **`vc_road_data.h`** $\rightarrow$ `.cameraPaths` (`s_CameraPath[]`): Camera movement boundaries, target tracking modes (`VC_MV_CHASE`, `VC_MV_SELF_VIEW`), lens flare types, and fixed angles.
*   **`chara_spawns.h`** $\rightarrow$ `.charaSpawnInfos` (`s_CharaSpawnInfo[]`): Enemy and NPC spawn positions, orientation angles, difficulty gating (`GameDifficulty_Easy`), and character IDs.
*   **`map_points.h`** $\rightarrow$ `.mapPoints` (`s_MapPoint2d[]`): 2D spatial waypoints for room transitions, doors, and event triggers.
*   **`header_field_D2C.h`** $\rightarrow$ `.collisionTriggers` (`s_CollisionTrigger[]`): Elevation step-height AABB volumes.

### Role of `extracted_data.c` (`extract_map_data.py`)
Located in `pc_port/build_gen/extracted_data/{map}_extracted_data.c`, these files are auto-generated (or manually maintained) by extracting raw symbol data from original PS1 binary overlays (`disc_extract/VIN/MAP*.BIN`).

**Why do they exist?**  
They carry data for symbols declared `extern` in the decomp source that have **not yet been fully reverse-engineered into native C headers**. Without them, the PC port would crash or suffer from broken logic:
*   `g_Cutscene_MapMsgAudioCmds` / `g_Cutscene_MapMsgAudioIdx`: Cutscene XA audio/voice playback commands (without these, cutscenes are silent).
*   `g_Cutscene_Timer`: Cutscene state machine timing scalars.
*   `g_Cutscene_CameraPosition` / `g_Cutscene_CameraLookAt`: Cutscene camera coordinates (`VECTOR3`).
*   `g_CommonWorldObjectPoses`: World pickup spawn coordinates and rotation (`s_WorldObjectPose[]`). (Without these, item pickups spawn at $(0,0,0)$ behind the camera).
*   `LOADABLE_INVENTORY_ITEMS`: Inventory item ID lists mapped to 3D TMD model slots.
*   `sharedData_800D1D14_3_s02` / `sharedData_800D1D1C_3_s02`: BGM volume limit caps and room layer flags.

---

## 5. Spatial Room System: `MAP_ROOM_IDXS` & `Map_RoomIdxGet`

A critical component of the overlay architecture is spatial tracking.

```
Player World Pos (X, Z) ──► Map_RoomIdxGet() ──► Grid Lookup (MAP_ROOM_IDXS) ──► Room ID (u8)
                                                                                       │
         ┌───────────────────────────────┬───────────────────────────────┐             │
         ▼                               ▼                               ▼             ▼
    BGM Volume Caps              Ambient SFX Task               Weather / Tint      Event Triggers
 (s_BgmLayerLimits)           (ambientAudioIdx)            (field_16 / field_17)
```

*   **Function:** `Map_RoomIdxGet(q19_12 posX, q19_12 posZ)` (in `include/maps/shared/Map_RoomIdxGet.h`).
*   **Grid:** `MAP_ROOM_IDXS` is a 2D lookup grid mapping world space (divided by `CHUNK_CELL_SIZE`) to a 1-byte **Room ID**.
*   **Engine Role:** As Harry moves, `Game_MapRoomIdxUpdate` calls `mapRoomIdxGet` every frame to determine the current Room ID. This ID controls BGM layer flags, ambient sound triggers, lighting/weather tints (`field_16` night tint, `field_17` rain/snow), and event script scoping.

---

## 6. Key Data Structures & File Locations Reference

| Component / Structure | C Type / Macro | File Location |
| :--- | :--- | :--- |
| **Master Overlay Header** | `typedef struct _MapOverlayHdr` (`SH_MAP_OVERLAY_HEADER`) | `external/SlickAmogus_silent-hill-decomp/include/bodyprog/map/map.h` |
| **Map Overlay Definitions** | `SH_MAP_OVERLAY_HEADER = { ... }` | `external/SlickAmogus_silent-hill-decomp/src/maps/mapX_sYY/mapX_sYY_header.c` |
| **Map Build System** | `add_map_overlay()` (CMake target generator) | `external/SlickAmogus_silent-hill-decomp/pc_port/maps/CMakeLists.txt` |
| **Room Index Resolver** | `Map_RoomIdxGet()`, `MAP_ROOM_IDXS[]` | `external/SlickAmogus_silent-hill-decomp/include/maps/shared/Map_RoomIdxGet.h` |
| **Extracted Overlay Data** | Static byte/struct arrays from PS1 `.BIN` | `external/SlickAmogus_silent-hill-decomp/pc_port/build_gen/extracted_data/` |
| **Extraction Script** | `extract_map_data.py` | `external/SlickAmogus_silent-hill-decomp/pc_port/tools/extract_map_data.py` |
| **IPD Collision Header** | `s_IpdCollisionData` | `docs/formats/COLLISION_FORMAT.md` & `src/pipeline/IpdToJsonConverter.cpp` |

---

## 7. Matrix of Knowns vs. Unknowns

This matrix outlines established facts vs. remaining unknowns for future cross-checking against upstream repository updates.

### Knowns (Verified & Established)
- [x] **Overlay Loading Mechanism:** Map overlays are compiled into standalone `.dll`/`.so` shared libraries and loaded dynamically by the executable via exported header symbols (`g_MapOverlayHeader_<name>`).
- [x] **Collision System Separation:** IPD handles continuous 2.5D sloped heightfield planes and vertical wall extrusions. Overlay (`s_CollisionTrigger`) handles discrete 3D AABB step-height snapping for kerbs/stairs.
- [x] **Overlay Data Mapping:** Camera paths (`vc_road_data.h`), spawns (`chara_spawns.h`), map waypoints (`map_points.h`), and step-height triggers (`header_field_D2C.h`) are fully decompiled into C header arrays.
- [x] **Room Spatial Grid:** `MAP_ROOM_IDXS` maps 2D world space to logical room IDs for BGM layer gating, ambient sound selection, and environment tints.
- [x] **ROM Repacking Requirement:** Custom maps require repacking the disc file allocation table so the engine filesystem driver recognizes newly added `.IPD` and `.TIM` files.

### Unknowns (To Be Cross-Checked Against Upstream Repositories)
- [ ] **Un-decompiled Struct Schemas in `extracted_data.c`:**
  - Full C struct definitions for cutscene voice command tables (`g_Cutscene_MapMsgAudioCmds`).
  - Full C struct layout for BGM room flag tables (`sharedData_800ED42C_4_s02` and `sharedData_800D1D1C_3_s02`).
  - Struct definition for boss rodata clusters (e.g. Floatstinger flight boundaries `D_800D780C` in `map4_s05`).
- [ ] **Unknown Header Fields in `_MapOverlayHdr`:**
  - `field_38` (`s_UnkStruct3_Mo*`): Exact purpose of this array (40 entries).
  - `field_4C` / `unkTable1_4C`: Secondary collision/mesh structure pointer?
  - `field_5C`, `field_7C`, `field_94`: Map-specific override pointers for maps like `map1_s01`, `map6_s04`, `map1_s02`.
- [ ] **Secondary Spatial Grid:**
  - `sharedData_800DF2DC_0_s00`: Full behavior of the secondary street grid used in outdoor maps (`MAP_HAS_SECONDARY_GRID` for `map0_s00`, `map0_s01`, `map2_s00`, `map2_s03`).
