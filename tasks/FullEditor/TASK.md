# Master Task: Unified C++ Level Editor (`FullEditor`)

## 1. Overview & Purpose

`FullEditor` serves as the **Master Task Channel** and central coordination hub for the Silent Hill 1 Level Editor. It tracks overarching architectural features, cross-module integration, system-wide UX improvements, and establishes direct linkages to all specialized subsystem tasks.

**Primary Goal:** Deliver a unified, performant C++ application (Raylib + ImGui) capable of full-lifecycle Silent Hill 1 level editing—ranging from low-poly geometry authoring and direct binary patching (`.IPD`, `.PLM`, `.TIM`) to map director configuration (waypoints, door links, camera paths, collision heightfields, enemy spawns, and room audio grids).

---

## 2. Specialized Subsystem Task Channels

Each major editor subsystem is tracked in its own dedicated task directory. Use this index for deep-dive specifications, implementation plans, and focused TODOs:

| Subsystem Task Channel | Focus & Scope | Status |
|---|---|---|
| **[`tasks/Dependencies/`](tasks/Dependencies/TASK.md)** | **Workspace File Manager**: Bidirectional dependency tracking (`dependencies.json` $\leftrightarrow$ `dependents.json`), asset library browsing, and safety locks against accidental deletion of in-use assets. | Active |
| **[`tasks/GlobalObjects/`](tasks/GlobalObjects/TASK.md)** | **PLM Object Manager**: Standalone inspection, placement, transform editing, and serialization for global `_GLB.PLM` prop libraries. | Active |
| **[`tasks/Waypoints/`](tasks/Waypoints/TASK.md)** | **Room Linkage Editor**: Interactive 3D door trigger volumes (`s_EventData`), destination waypoints (`s_MapPoint2d`), directed spline visualizers, and map transition wiring. | Active |
| **[`tasks/BinaryOverlays/`](tasks/BinaryOverlays/TASK.md)** | **Binary Overlay Analysis**: Per-map binary director overlays (`VIN/MAP*_S**.BIN`), C struct schema mapping, and dynamic library linking for custom map code. | Research / Active |
| **[`tasks/Audio/`](tasks/Audio/TASK.md)** | **Audio Player & Environmental Soundscapes**: Parsing and playback for PS1 soundbanks (`.VAB`), MIDI sequences (`.SEQ`), and room-based BGM layer volume mixers. | Planning |
| **[`tasks/ContextMenus/`](tasks/ContextMenus/Suggested%20Context%20Menus.md)** | **Contextual Menus & Quick Actions**: Viewport right-click radial/context menus for copy/paste, geometry alignment, UV resets, and mesh operations. | In Progress |

---

## 3. Master Phased Roadmap

The high-level technical evolution of the editor is structured across five major phases (see [`Planned Features.md`](tasks/FullEditor/Planned%20Features.md) for full data structure specifications):

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                             MASTER PHASE ROADMAP                                 │
├───────────────────┬──────────────────────────────────────────────────────────────┤
│ Phase 1: Viewport │ • Layer management toolbar (Mesh, Physics, Triggers, Rails)  │
│ Layer System      │ • Collision step AABB boxes (header_field_D2C.h)             │
│ & Step AABBs      │ • Camera path triggers & rail boxes (vc_road_data.h)         │
├───────────────────┼──────────────────────────────────────────────────────────────┤
│ Phase 2: Entity   │ • 3D Transform gizmos for enemy spawns (chara_spawns.h)      │
│ Gizmos & Pickups  │ • 3D PLM prop rendering for world pickups (WorldObjectPoses) │
│                   │ • Difficulty filter toggles (Easy / Hard)                    │
├───────────────────┼──────────────────────────────────────────────────────────────┤
│ Phase 3: Door     │ • Interactive 3D waypoint placement (s_MapPoint2d)           │
│ Links & Waypoints │ • Door trigger boxes & directed 3D spline linkages           │
│                   │ • Bidirectional link editor and Overlay JSON/C roundtrip     │
├───────────────────┼──────────────────────────────────────────────────────────────┤
│ Phase 4: Spatial  │ • 2D/3D top-down grid painter for MAP_ROOM_IDXS              │
│ Grid & Env Audio  │ • Environment panel (fog distance, lighting tint, weather)   │
│                   │ • 8-layer BGM volume mixer per room index                    │
├───────────────────┼──────────────────────────────────────────────────────────────┤
│ Phase 5: Exporters│ • C decomp header generator (vc_road_data, chara_spawns, etc)│
│ & Repacker Bridge │ • Atomic binary IPD repacker and ROM disc table update tool  │
└───────────────────┴──────────────────────────────────────────────────────────────┘
```

---

## 4. Current Subsystem Architecture & Status

### A. 3D Viewport & Camera Engine (`include/viewport/`)
- **Status:** **Complete & Stable.**
- Implemented in `Viewport` with `ViewportBase` orbit/pan/zoom controls, camera persistence, coordinate grids, and frustum culling (`Frustum.h`).
- Multi-overlay architecture routes rendering, picking, and context menus to active modes (`LocalGeometryOverlay`, `CollisionOverlay`, `WaypointsOverlay`).

### B. Direct Binary Geometry Pipeline (`include/core/`, `include/geometry/`)
- **Status:** **Complete & Active.**
- Fast parsing via `IPDParse.h` and byte-exact in-place write-back via `IPDWrite.h`.
- Full Blender-inspired 3D modeling toolset in `LocalGeometryPanel` across 4 modes (Global Objects, Meshes, Faces, Vertices).
- PlayStation hardware constraints validation in `ChunkValidator.h` (255-vertex limits, 16-unit height bounds, 40-unit cell bounds).
- Deep-copy transactional undo/redo engine in `History.h`.

### C. Texture Mapping & 2D CLUT Canvas (`include/panels/`, `include/core/`)
- **Status:** **Active.**
- 2D texture palette browser in `TextureMapPanel` with CLUT row switching and 3D face tile painting.
- 2D UV canvas in `TextureEditPanel` for pixel-level inspection and UV coordinate adjustment.
- GPU texture caching via `TextureCache` in `Textures.h`.

### D. Outliner & Scene Hierarchy (`include/panels/OutlinerPanel.h`)
- **Status:** **Active.**
- Treeview of loaded chunks, objects, and submeshes with visibility toggling and selection synchronization.

### E. Map Registry & Overlay Director (`include/core/`, `include/panels/MapsPanel.h`)
- **Status:** **Active.**
- 43-map registry (`MapTable.h`) with search filtering, overlay loading (`OverlayLoader.h`), and 3D waypoint visualization.

### F. Python Asset Pipeline Bridge (`scripts/`, `include/core/AssetManager.h`)
- **Status:** **Complete & Active.**
- Lean, modular Python core (`scripts/core/`) wrapped natively by C++ `AssetManager` for extraction, deployment, and reversion.

---

## 5. Overarching Backlog & Selected Action Items

See [`TODO.md`](tasks/FullEditor/TODO.md) and [`Suggested Improvements.md`](tasks/FullEditor/Suggested%20Improvements.md) for individual work tickets:

### High Priority
- [ ] **Viewport Layer Manager**: Add unified layer visibility toggles (Mesh, Collision, Waypoints, Camera Rails, Spawns) in the top viewport bar.
- [ ] **Scene Outliner Camera Centering**: Double-clicking or clicking a chunk/object link in the Outliner should orbit-center the camera onto its centroid.
- [ ] **CLUT Row Visualizer**: Add a visual 16-color palette stripe picker in `TextureMapPanel` to complement the current integer slider.
- [ ] **Texture Aspect Ratio**: Correct 128x256 TIM textures to render at half width relative to 256x256 textures in UV editors.
- [ ] **Missing Geometry Validator**: Add sanity check alerting the user if a loaded map waypoint/door references a chunk coordinate with no existing `.IPD` geometry.

### Medium Priority
- [ ] **Expand/Collapse All**: Add bulk expand/collapse controls to the Scene Outliner hierarchy tree.
- [ ] **Reset UV Context Action**: Add a "Reset to Default Planar UV" entry to the viewport right-click context menu.
- [ ] **Multiselect Refinement**: Support Shift-click range selection and Box Drag selection across Face and Vertex modes.
- [ ] **Auto-generate Project Folders**: Add a "Generate Workspace Folders" button in `SettingsPanel` to pre-populate missing subdirectories on initial setup.

---

## 6. Key Documentation References

- **Architecture Index:** [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- **Detailed Feature Specs:** [`tasks/FullEditor/Planned Features.md`](tasks/FullEditor/Planned%20Features.md)
- **Improvement Ideas & Backlog:** [`tasks/FullEditor/Suggested Improvements.md`](tasks/FullEditor/Suggested%20Improvements.md)
- **Binary Format Truths:** [`docs/formats/IPD.md`](docs/formats/IPD.md), [`docs/formats/Collision.md`](docs/formats/Collision.md), [`docs/formats/Binary Overlays.md`](docs/formats/Binary%20Overlays.md)
- **Decompiled Source References:** `game/PC/` (`map_registry.c`, `events_main.c`, `vc_road_data.h`, `chara_spawns.h`)
