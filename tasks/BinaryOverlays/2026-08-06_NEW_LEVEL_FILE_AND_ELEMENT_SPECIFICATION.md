# Complete Specification for Custom Level Authoring: Files, Data Structures & Engine Elements

**Date:** 2026-08-06  
**Status:** Comprehensive Specification  
**Task:** `tasks/binary_overlay_analysis`  

---

## 1. Architectural Overview

Creating an entirely new custom level in Silent Hill 1 (and the PC Port) requires defining two distinct pillars:
1. **Streamed Asset Files (`./gamedata` or ROM Disc Sectors):** The binary media containing 3D meshes, 2.5D floor/wall collision, textures, and audio samples.
2. **Logic & Overlay Data (`maps/mapX_sYY.dll` / C Source):** The code and data structures contained within the `s_MapOverlayHdr` master struct (`SH_MAP_OVERLAY_HEADER`) that dictate camera behavior, event triggers, enemy spawns, dialogue, audio mapping, and fog/lighting settings.

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                             CUSTOM LEVEL DEFINITION                             │
├────────────────────────────────────────┬────────────────────────────────────────┤
│          PILLAR 1: ASSET FILES         │         PILLAR 2: OVERLAY HEADER       │
│        (Streamed via filesystem)       │       (Compiled into mapX_sYY.dll)     │
├────────────────────────────────────────┼────────────────────────────────────────┤
│ • Level Geometry & Height (.IPD)       │ • Camera Paths & Triggers              │
│ • 3D Prop Models (.PLM)                │ • Dialogue & Examine Strings           │
│ • Textures (.TIM / .PNG)               │ • Step-Height Elevation AABBs          │
│ • Soundbanks (.VAB/.VH/.VB & XA)       │ • Enemy & Item Spawns / Item Poses     │
│ • Disc File Allocation Table (Repacked)│ • Blood Splats & Particle Systems      │
│                                        │ • Fog, Lighting Tint & Weather         │
│                                        │ • BGM / Ambient Audio Mapping          │
│                                        │ • Door Waypoints & Scripted Events     │
│                                        │ • Spatial Room Grid (Map_RoomIdxGet)   │
└────────────────────────────────────────┴────────────────────────────────────────┘
```

---

## 2. Pillar 1: Streamed Asset Files Specification

| Element | File Type / Extension | Location | Purpose & Engine Role |
| :--- | :--- | :--- | :--- |
| **Map Geometry & Physics** | `.IPD` | `./gamedata/` or Disc `ST/` | Contains the visual 3D chunk meshes, material assignments, and embedded 2.5D heightfield physics (`s_IpdCollisionData`). |
| **3D Prop Models** | `.PLM` | `./gamedata/` or Disc `ST/` | 3D models for static world props, doors, puzzles, and collectible inventory items. |
| **Textures & UI** | `.TIM` / `.PNG` | `./gamedata/` or Disc `ST/` | VRAM texture pages, CLUT palette data, decal sheets, and menu graphics. |
| **Sound Effects / VAB** | `.VH` (Header), `.VB` (Body) | `./gamedata/` or Disc `ST/` | VAB soundbanks containing audio sample data for footsteps, ambient sound effects, and monster noises. |
| **Streaming Voice / BGM** | `.XA` / `.WAV` | `./gamedata/` or Disc `XA/` | Streamed compressed audio tracks for cutscene dialogue voice lines and background music tracks. |
| **ROM File Table Index** | System Index (Allocation Table) | ROM Image / System File Table | **CRITICAL:** When adding brand-new `.IPD`, `.PLM`, or `.TIM` filenames, the disc allocation table/index must be repacked so the engine's file system driver recognizes the file entries. |

---

## 3. Pillar 2: Master Overlay Header (`s_MapOverlayHdr`) Specification

The map overlay header (`SH_MAP_OVERLAY_HEADER`, defined as `struct _MapOverlayHdr` in `external/SlickAmogus_silent-hill-decomp/include/bodyprog/map/map.h`) controls all map behavior. Below is the complete specification of every element required for a new level.

### A. Camera Systems (`cameraPaths`)
*   **Field:** `VC_ROAD_DATA cameraPaths[CAMERA_PATH_COUNT_MAX]`
*   **Header File:** `vc_road_data.h`
*   **Data Structure:** `s_CameraPath` (`VC_ROAD_DATA`)
*   **Elements Defined:**
    *   `lim_sw`: Trigger volume box (`min_hx`, `max_hx`, `min_hz`, `max_hz`) defining where the player must be standing to activate this camera zone.
    *   `lim_rd`: Camera rail/position bounds where the physical camera is permitted to move.
    *   `cam_mv_type` / `mv_y_type`: Movement tracking behavior (e.g. `VC_MV_CHASE` for follow camera, `VC_MV_SELF_VIEW` for fixed angle).
    *   `fix_ang_x`, `fix_ang_y`: Fixed rotation pitch and yaw angles for static camera views.
    *   `lens_flare`: Type of lens flare effect enabled for this camera zone.

### B. Dialogue & Examine Strings (`mapMessages`)
*   **Field:** `const char** mapMessages`
*   **Header / Array:** Local string array in `mapX_sYY_header.c`
*   **Elements Defined:**
    *   Null-terminated array of string pointers (`const char* MAP_MESSAGES[]`).
    *   Contains all text displayed when examining objects, reading notes, or during dialogue cutscenes.
    *   Referenced by event scripts via message index (e.g. `Map_MessageDisplay(msgIdx)`).

### C. Collision Systems
*   **Primary Physics (Floors & Walls):**
    *   Defined inside the `.IPD` file header (`s_IpdCollisionData`).
    *   Provides continuous 2.5D sloped planes (`baseGroundHeight`, `tiltAngleX`, `tiltAngleZ`) and extruded vertical walls (`disableHeight = true`).
*   **Step-Height Elevation AABBs (`collisionTriggers`):**
    *   **Field:** `s_CollisionTrigger collisionTriggers[COLLISION_TRIGGER_COUNT_MAX]`
    *   **Header File:** `header_field_D2C.h`
    *   **Elements Defined:** Axis-Aligned Bounding Boxes (AABBs) specifying `(minX, maxX, minZ, maxZ)` bounds and a target $Y$ height. Used to snap Harry's elevation on stairs, curbs, and ledges where sloped planes would clip.

### D. Item & Enemy Placement
1.  **Enemy & NPC Spawns (`charaSpawnInfos` & `charaGroupIds`):**
    *   **Field:** `s_SpawnInfo charaSpawnInfos[2][16]` & `s8 charaGroupIds[2]`
    *   **Header File:** `chara_spawns.h`
    *   **Elements Defined:** Enemy character IDs (e.g., `Chara_PuppetNurse`, `Chara_Creeper`), 3D spawn coordinates $(X, Y, Z)$ fixed-point `Q12`, spawn rotation angle `Q8_ANGLE`, difficulty filter (`GameDifficulty_Easy`/`Hard`), and activation flags.
2.  **Loadable Item 3D Models (`loadableItems`):**
    *   **Field:** `u8* loadableItems`
    *   **Array:** `LOADABLE_INVENTORY_ITEMS` (in `extracted_data.c` or header)
    *   **Elements Defined:** Array of item IDs (e.g., Handgun, First Aid Kit, Key). Tells the engine which 3D `.PLM` models to load into TMD memory slots for rendering world pickups.
3.  **World Pickup Poses (`g_CommonWorldObjectPoses`):**
    *   **Field:** `s_WorldObjectPose g_CommonWorldObjectPoses[]`
    *   **Elements Defined:** Exact 3D position vector (`VECTOR3`) and rotation vector (`SVECTOR3`) for pickup items floating/placed in the level.

### E. Decals, Blood Splats & Particle FX
*   **Fields:** `s_BloodSplat* bloodSplats`, `s16 bloodSplatCount`, `particlesUpdate`
*   **Elements Defined:**
    *   `bloodSplats`: Array of static blood splat decal positions, scales, and rotation angles placed on floors/walls.
    *   `particlesUpdate`: Callback function (`Particle_SystemUpdate`) managing weather particles (rain, snow, dust specks, ash).

### F. Environment, Fog, Lighting & Weather
*   **Ambient Tint / Lighting Mode (`field_16`):**
    *   `0`: Standard daylight / indoor lighting.
    *   `2`: Hallway intro tint (otherworld transition glow).
    *   `3`: **Night / Otherworld Dark Mode** (activates full flashlight fog cone and dark ambient tint).
*   **Weather System (`field_17`):**
    *   Specifies active weather effect: `0 = Clear`, `1 = Rain`, `2 = Heavy Rain`, `3 = Snow`.
*   **Wind Vectors (`windSpeedX`, `windSpeedZ`):**
    *   Pointers to fixed-point scalars controlling wind velocity applied to weather particles and fog drift.

### G. Music & Audio Track Mapping
*   **BGM Track Index (`bgmIdx`):** `s8 bgmIdx` - Index of the primary background music track to play upon map load.
*   **Ambient VAB Index (`ambientAudioIdx`):** `u8 ambientAudioIdx` - Index of the ambient soundbank task (e.g. industrial hums, wind noise).
*   **BGM Event Callback (`bgmEvent`):** Pointer to `Map_RoomBgmInit` function.
*   **BGM Room Layer Caps & Flags:**
    *   `s_BgmLayerLimits` (`sharedData_*`): 8-byte array defining maximum volume caps $(0-128)$ for each BGM audio stem/layer per room.
    *   `sharedData_*` (u16 Room Flags): Bitmask table mapping Room IDs to active BGM audio layers (e.g. enabling drum layer when entering a specific room).

### H. Scripting, Doors & Navigation Waypoints
1.  **Map Waypoints (`mapPoints`):**
    *   **Field:** `s_MapPoint2d* mapPoints`
    *   **Header File:** `map_points.h`
    *   **Elements Defined:** 2D spatial coordinate points (`s_MapPoint2d`) used for spawn arrival locations, door target destinations, and cutscene actor positioning.
2.  **Event Data (`mapEvents` & `mapEventFuncs`):**
    *   **Fields:** `s_EventData* mapEvents`, `void (**mapEventFuncs)()`
    *   **Elements Defined:** 
        *   `mapEvents`: Array of trigger boxes (`s_EventData`) defining activation bounds, trigger types (e.g. `TriggerType_Door`, `TriggerType_Examine`, `TriggerType_Zone`), and target event function indices.
        *   `mapEventFuncs`: Array of C function pointers (`MapEvent_DoorJammed`, `MapEvent_PuzzleSolve`, etc.) executing the logic when a trigger is activated.

### I. Spatial Room Grid (`mapRoomIdxGet` & `MAP_ROOM_IDXS`)
*   **Field:** `u8 (*mapRoomIdxGet)(q19_12 posX, q19_12 posZ)`
*   **Header File:** `include/maps/shared/Map_RoomIdxGet.h`
*   **Elements Defined:**
    *   `MAP_ROOM_IDXS`: 2D grid array mapping physical world space coordinates $(X, Z)$ to 1-byte **Room IDs**.
    *   `Map_RoomIdxGet()`: Function resolving player position to a Room ID. Used every frame to evaluate BGM layer changes, lighting switches, and ambient sound triggers.

---

## 4. Complete Directory Layout for a Custom Map

To construct a new level named `mapX_sYY`, create the following file layout in the workspace:

```
external/SlickAmogus_silent-hill-decomp/
├── src/maps/mapX_sYY/
│   ├── mapX_sYY_header.c        <-- Master SH_MAP_OVERLAY_HEADER definition
│   ├── mapX_sYY_events.c        <-- Scripted C functions for doors/puzzles/cutscenes
│   ├── vc_road_data.h           <-- Camera paths array (s_CameraPath[])
│   ├── chara_spawns.h           <-- Enemy spawn array (s_SpawnInfo[])
│   ├── map_points.h             <-- 2D map waypoints (s_MapPoint2d[])
│   └── header_field_D2C.h       <-- Step-height elevation AABBs (s_CollisionTrigger[])
│
├── pc_port/build_gen/extracted_data/
│   └── mapX_sYY_extracted_data.c <-- RoData, item poses, audio tables, BGM limit caps
│
├── pc_port/maps/
│   └── CMakeLists.txt           <-- Add: add_map_overlay(mapX_sYY MAPX_SYY)
│
└── gamedata/ (or ROM Disc Image ST/ folder)
    ├── MAPX_SYY.IPD             <-- 3D Mesh + 2.5D Heightfield Physics
    ├── MAPX_SYY.TIM             <-- VRAM Textures & CLUTs
    ├── PROPS.PLM                <-- Custom 3D Prop Models
    └── SYSTEM_FILE_TABLE        <-- Repacked ROM Disc Allocation Table
```

---

## 5. Level Authoring Checklist

When authoring a custom level from scratch, verify every item on this checklist:

- [ ] **Geometry & Textures:** Model exported to `.IPD` with valid subcell physics and wall flags; textures exported to `.TIM`.
- [ ] **ROM Table:** Disc file allocation table repacked if adding new file entries.
- [ ] **Camera System:** `vc_road_data.h` authored with trigger volumes covering every playable tile.
- [ ] **Spatial Grid:** `MAP_ROOM_IDXS` 2D grid defined in `Map_RoomIdxGet.h` mapping $X/Z$ space to Room IDs.
- [ ] **Audio Setup:** `bgmIdx`, `ambientAudioIdx`, BGM layer limits (`s_BgmLayerLimits`), and room layer flags populated.
- [ ] **Lighting & Fog:** `field_16` (tint/night mode) and `field_17` (weather) configured.
- [ ] **Collision Triggers:** `header_field_D2C.h` authored for all stairs, curbs, and ledges.
- [ ] **Spawns & Pickups:** `chara_spawns.h` populated for monsters; `loadableItems` and `g_CommonWorldObjectPoses` defined for item pickups.
- [ ] **Events & Waypoints:** `map_points.h` and `mapEvents` authored for all doors, transitions, and examine points.
- [ ] **Build Target:** `add_map_overlay(mapX_sYY ...)` added to `pc_port/maps/CMakeLists.txt` to compile `maps/mapX_sYY.dll`.
