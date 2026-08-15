# Planned Features & Frontend Mapping Specification (`unified_cpp_editor`)

**Status:** Feature Architecture & Implementation Plan  
**Target Application:** `unified_cpp_editor` (C++ Raylib + ImGui Frontend & Python Scripting Pipeline)  

---

## 1. Overview & Vision

The `unified_cpp_editor` is the central tool for viewing, editing, and authoring Silent Hill 1 map environments. To support full level editing—ranging from simple object repositioning to building entirely new custom maps—the editor must visually map, interactively manipulate, and round-trip serialize all level systems.

This document details the planned frontend features across 7 core map subsystems. For each feature, it specifies:
1. **Source Data & Structures:** Where the data lives (raw binary, C header structs, `.IPD` headers).
2. **Visual Viewport Representation:** How Raylib renders the feature in 3D.
3. **Interactive Editing Controls:** How the user manipulates it in ImGui / 3D Gizmos.
4. **Read/Write Pipeline:** Python scripting bridge (`tooling/scripts/`) and direct C++ native implementation.

---

## 2. Feature Specification Matrix

### Feature 1: Dual Collision System (IPD Heightfield + Overlay Step AABBs)

*   **Source Data:**
    *   *IPD Base Physics:* `.IPD` file header `s_IpdCollisionData` (Subcells, split vertices, `s_IpdCollSurface` floor planes & `disableHeight` wall flags).
    *   *Overlay Step Boxes:* `header_field_D2C.h` (`s_CollisionTrigger collisionTriggers[]`).
*   **Visual Viewport Representation:**
    *   **IPD Planes:** Render 2.5D floor polygons color-coded by surface/footstep SFX type (e.g. Green = Dirt, Grey = Concrete, Yellow = Metal Grate, Blue = Wood). Render vertical wall lines as extruded translucent 3D planes (`disableHeight = true`).
    *   **Overlay Step Boxes:** Render translucent 3D wireframe boxes (cyan/magenta) representing AABB step-height elevation triggers for curbs and stairs.
*   **Interactive Editing Controls:**
    *   3D Translate/Scale gizmos for adjusting step-height AABB volumes (`minX, maxX, minZ, maxZ, targetY`).
    *   Vertex and split-line manipulation for IPD subcell bisectors.
    *   Inspector dropdown for surface ground types (Footstep SFX selection).
*   **Read/Write Pipeline:**
    *   *Python Bridge:* `tooling/parsers/ipd_dump.py` exports/imports IPD collision payload to JSON. Python script for `header_field_D2C.h` array extraction.
    *   *C++ Direct:* `src/viewport/CollisionViewport.cpp` using Raylib `DrawCubeWires()`, `DrawTriangle3D()`, and `DrawBoundingBox()`.

---

### Feature 2: Camera Paths & Rail Tracking

*   **Source Data:** `vc_road_data.h` (`VC_ROAD_DATA cameraPaths[CAMERA_PATH_COUNT_MAX]`).
*   **Visual Viewport Representation:**
    *   **Trigger Box (`lim_sw`):** Yellow/Gold 3D wireframe bounding box indicating where Harry must stand to activate the camera zone.
    *   **Camera Rail/Bounds (`lim_rd`):** Blue 3D wireframe bounding box/line showing where the physical camera is allowed to move.
    *   **Aim Vector / Frustum:** Render a 3D frustum pyramid or target line displaying fixed orientation angles (`fix_ang_x`, `fix_ang_y`) or target tracking vector.
*   **Interactive Editing Controls:**
    *   Dual-box 3D gizmos (manipulate trigger volume vs. camera movement bounds independently).
    *   Inspector dropdowns for `cam_mv_type` (`VC_MV_CHASE`, `VC_MV_SELF_VIEW`), `mv_y_type`, and `lens_flare`.
    *   "Camera Preview Window": A picture-in-picture viewport showing what the game camera actually sees from the current camera path config.
*   **Read/Write Pipeline:**
    *   *Python Bridge:* `tooling/scripts/camera_path_parser.py` (reads `vc_road_data.h` $\leftrightarrow$ JSON).
    *   *C++ Direct:* `src/viewport/CameraViewport.cpp` with interactive Raylib camera frustum rendering and header code generation.

---

### Feature 3: Linking Rooms, Doors & Waypoints

*   **Source Data:**
    *   `map_points.h` (`s_MapPoint2d mapPoints[]`).
    *   `mapEvents` (`s_EventData[]`) & `mapEventFuncs`.
*   **Visual Viewport Representation:**
    *   **Door/Zone Triggers:** Green 3D wireframe AABB volumes for `mapEvents`.
    *   **Waypoints:** 3D Waypoint pins/billboards at $(X, Z)$ positions (`s_MapPoint2d`).
    *   **Door Linking Curves:** Render directed 3D Bezier splines/lines connecting a door trigger in Room A to its destination waypoint in Room B.
*   **Interactive Editing Controls:**
    *   Click-and-drag 3D waypoint placement.
    *   "Link Door" tool: Click a door event box, then click a target waypoint pin to automatically update the event target index and coordinates.
    *   Inspector for event trigger types (`TriggerType_Door`, `TriggerType_Examine`, `TriggerType_Zone`).
*   **Read/Write Pipeline:**
    *   *Python Bridge:* `tooling/scripts/event_parser.py` converting event tables and `map_points.h` to editable JSON.
    *   *C++ Direct:* Native rendering in `src/viewport/EventViewport.cpp` emitting updated C arrays.

---

### Feature 4: Enemy & Item Spawns / World Pickups

*   **Source Data:**
    *   `chara_spawns.h` (`s_SpawnInfo charaSpawnInfos[2][16]`).
    *   `LOADABLE_INVENTORY_ITEMS` (`u8* loadableItems`).
    *   `g_CommonWorldObjectPoses` (`s_WorldObjectPose[]`).
*   **Visual Viewport Representation:**
    *   **Enemy Spawns:** 3D monster bounding capsules or low-poly proxy meshes color-coded by difficulty filter (`GameDifficulty_Easy`/`Hard`). Includes a 3D heading arrow showing initial spawn rotation angle (`Q8_ANGLE`).
    *   **World Item Pickups:** Load and render the actual 3D `.PLM` model (e.g. Handgun, Health Drink, Key) at its exact `VECTOR3` position and `SVECTOR3` rotation.
*   **Interactive Editing Controls:**
    *   3D Transform Gizmo (Translate position, Rotate heading).
    *   Dropdown pickers for Enemy Type (`Chara_PuppetNurse`, `Chara_Creeper`, etc.) and Item Type.
    *   Duplicate / Add Spawn button.
*   **Read/Write Pipeline:**
    *   *Python Bridge:* `tooling/scripts/spawn_parser.py` converting `chara_spawns.h` $\leftrightarrow$ JSON.
    *   *C++ Direct:* `src/viewport/SpawnViewport.cpp` integrating Raylib `.PLM` mesh rendering and gizmos.

---

### Feature 5: Blood Splats, Decals & Particle Systems

*   **Source Data:**
    *   `bloodSplats` (`s_BloodSplat* bloodSplats`, `bloodSplatCount`).
    *   `particlesUpdate` callback & weather parameters.
*   **Visual Viewport Representation:**
    *   **Blood Splats:** Projected 2D texture quads mapped onto IPD floor and wall geometry, showing exact position, orientation, and scale.
    *   **Particle Emitters:** Animated 3D particle preview showing weather effects (rain streaks, snowflakes, dust specks, ash).
*   **Interactive Editing Controls:**
    *   "Decal Stamp Tool": Click anywhere on IPD geometry to stamp a blood splat decal; drag handles to scale/rotate.
    *   Particle System toggle and weather intensity sliders in ImGui.
*   **Read/Write Pipeline:**
    *   *Python Bridge:* JSON serializer for `s_BloodSplat[]` arrays.
    *   *C++ Direct:* Decal projection shaders (`src/viewport/DecalViewport.cpp`) emitting C struct arrays for `bloodSplats`.

---

### Feature 6: Environment, Fog, Lighting & Weather

*   **Source Data:**
    *   `field_16` (Ambient tint & draw distance / night mode).
    *   `field_17` (Weather type: Clear, Rain, Heavy Rain, Snow).
    *   `windSpeedX`, `windSpeedZ`.
*   **Visual Viewport Representation:**
    *   **Dynamic Viewport Shading:** Real-time GLSL shader simulating:
        *   `field_16 = 3`: Night / Otherworld Mode (dark ambient tint + active 3D flashlight cone attached to viewport camera).
        *   `field_16 = 2`: Otherworld hallway transition glow/tint.
        *   `field_16 = 0`: Standard daylight / indoor lighting.
    *   **Weather Simulation:** Viewport particle overlay simulating rain or snow driven by `field_17` and wind vectors (`windSpeedX/Z`).
*   **Interactive Editing Controls:**
    *   ImGui Environment Panel with:
        *   Ambient Tint Color Picker.
        *   Fog Start / End Distance Sliders.
        *   Weather Type Radio Buttons (Clear / Rain / Heavy Rain / Snow).
        *   Wind Vector 2D Joystick Control.
*   **Read/Write Pipeline:**
    *   *Direct C++:* Immediate property binding to `SH_MAP_OVERLAY_HEADER` fields in memory.

---

### Feature 7: Audio Mapping & Spatial Room Grid

*   **Source Data:**
    *   `MAP_ROOM_IDXS` 2D grid & `Map_RoomIdxGet.h`.
    *   `bgmIdx`, `ambientAudioIdx`, `s_BgmLayerLimits`, Room Flag Tables.
*   **Visual Viewport Representation:**
    *   **Spatial Grid Overlay:** Top-down 2D/3D grid overlay (colored cells) representing `MAP_ROOM_IDXS`. Each grid cell is color-coded by its assigned **Room ID**.
    *   **Audio Inspector:** Hovering over any cell displays a popup listing:
        *   Assigned Room ID.
        *   BGM Track Name & active audio stems (e.g. Drums, Bass, Melody).
        *   BGM Layer Volume Caps ($0-128$).
        *   Ambient Soundbank Profile.
*   **Interactive Editing Controls:**
    *   "Room Paintbrush Tool": Select a Room ID and paint grid cells in 2D/3D top-down view.
    *   BGM Audio Mixer Panel: ImGui multi-slider panel to adjust volume caps for all 8 BGM layers per room.
*   **Read/Write Pipeline:**
    *   *Python Bridge:* `tooling/scripts/room_grid_parser.py` parsing/generating `MAP_ROOM_IDXS` grid arrays and inline `Map_RoomIdxGet.h` code.
    *   *C++ Direct:* Native grid painter in `src/viewport/AudioGridViewport.cpp`.

---

## 3. Viewport Layer Architecture & UI Layout

To prevent visual clutter, the `unified_cpp_editor` viewport will implement a modular **Layer Manager**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ VIEWPORT LAYER MANAGER                                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│  [X] Map Mesh (.IPD Visuals)             [ ] Blood Splats & Decals          │
│  [X] IPD Physics Planes & Walls          [ ] Weather & Fog Effects          │
│  [ ] Step-Height Elevation AABBs         [X] Door Triggers & Waypoints      │
│  [X] Camera Paths & Frustums             [ ] Spatial Room Grid (Audio/Tint) │
│  [X] Enemy & Item Spawns                 [ ] Particle Emitters              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Viewport Modes:
1. **Geometry & Collision Mode:** Focuses on `.IPD` mesh, floor slopes, wall lines, and step-height AABBs.
2. **Director & Scripting Mode:** Focuses on camera paths, door links, waypoints, and event trigger boxes.
3. **Entity & Placement Mode:** Focuses on enemy spawns, 3D item pickups, and blood splat decals.
4. **Environment & Audio Mode:** Focuses on spatial room grid painting, fog/lighting sliders, BGM layer mixers, and weather simulation.

---

## 4. Implementation Roadmap

| Phase | Milestone | Core Tasks |
| :--- | :--- | :--- |
| **Phase 1** | **Viewport Layer System** | Implement layer toggle toolbar in Raylib viewport; add wireframe rendering for step-height AABBs and camera paths (`vc_road_data.h`). |
| **Phase 2** | **Entity Gizmos & Pickups** | Add 3D Translate/Rotate gizmos for enemy spawns (`chara_spawns.h`) and render `.PLM` 3D models for item pickups (`g_CommonWorldObjectPoses`). |
| **Phase 3** | **Door Linking & Scripting** | Implement visual spline links between door event boxes and target `mapPoints`; add ImGui event inspector. |
| **Phase 4** | **Spatial Grid & Environment** | Add top-down room grid painter for `MAP_ROOM_IDXS`; implement ImGui environment panel for fog, lighting tint, and weather. |
| **Phase 5** | **Code & Binary Exporters** | Build C source exporter (emits `vc_road_data.h`, `chara_spawns.h`, `map_points.h`, `mapX_sYY_header.c`) and `.IPD`/ROM repacker bridge. |

---

## 5. Proposed UI Schematic (Workspace Layout)

**LEFT**
- **Chunks**: (Current 'Chunk Manager'). Selects chunks to perform scripts on and load into the viewport. A crucial part of the conversion process, should be persistent. *Note: IPD inspector will be integrated here, so all dependencies for PLMs, TIMs, etc. are included in this same panel.*

**CENTER (Tabbable)**
- **Scene**: A simple layout viewer in its current form.
- **Local Geometry**: (Current 'Edit' mode). Allows plopping in Global Objects related to the chunk of interest.
- **Global Geometry**: Disconnected viewport that serves as a PLM manager/editor.
- **Collision**
- **Spawns**
- **Camera**: Takes occlusion wall logic from current 'Collision' viewport to simplify workflow.
- **Audio**
- **Maps**: Environment, paper maps, and any other map-specific components unaccounted for.
- **Texture Editor**: Allows for specific management of textures and CLUTs, importing PNGs into TIMs with validation/handling, exporting/importing existing textures, actual texture painting, and different modes (e.g., actual vs raw mode) to view each tile/UV quadrilateral to their relevant CLUT row.

**BOTTOM**
- **Console**: Keep this panel simple.

**RIGHT (Split vertically into two panels)**
- **Outliner**: Tree list of what is currently loaded into the scene, selectable with aliases.
- **Textures**: (In the same tab group as Outliner). Current 'Texture Manager' that allows for applied texture painting, redrawing UV maps, viewing palettes. Needs to be connected to the 'Texture Editor' in the central panel.
- **Tools**: Adaptive to the center panel, always visible.
