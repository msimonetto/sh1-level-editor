# TODO — Unified C++ Editor

## Selected Minor Changes (`Suggested Improvements.md`)
- [x] Reorganise dropdown order (Maps, then Actions, then stub 'Other') in 'Maps' panel.
- [x] Refine and standardize clunky enum map names.
- [x] Remove missing map prefix options from Global Geometry dropdowns, include other non-`GLB` suffix PLM files.
- [x] Limit height of 'Maps' panel dropdown (table of 'Maps') to bottom of lowest entry.
- [x] Increase camera up/down move speed relative to WASD move speed.
- [ ] Fix default camera position on startup (decouple from selected map).
- [ ] Add toggles to hide/show specific viewports in the toolbar under a new toolbar tab 'Panels', including the internal console.
- [ ] Add an 'Expand/collapse all' button to the Scene Outliner.
- [ ] Generate `workspace`/`assets` folder layout upon configuration selection and button press (Generate Folders, in 'Settings').
- [ ] 128x256 textures should be half width (i.e., tiles same size as in 256x256 relative to panel width).
- [ ] Add a 'Reset to default' button for face UVs (selectable in right click menu).
- [ ] Make Outliner chunk hyperlinks center the camera position and angle.
- [ ] Add a visual CLUT row viewer to complement the current slider in Texture Map.
- [ ] Validator: Check if chunks with waypoints/doors actually have geometry chunk data.
- [ ] Hide/show terminal (background, not console within window) from options.
- [ ] Add texture individual UV point change (middle click + drag).

## Planned Features (later)

See `PLANNED_FEATURES.md` for full technical specifications and data pipelines. These tie heavily into an understanding of the binary overlays (likely requires the source code to be rebuilt).

- [ ] **Phase 1: Viewport Layer System & Step AABBs**
  - Add layer toggle UI in Raylib toolbar.
  - Implement 3D wireframe box rendering for `s_CollisionTrigger` step-height AABBs (`header_field_D2C.h`).
  - Render camera path trigger boxes (`lim_sw`) and movement rails (`lim_rd`) from `vc_road_data.h`.

- [ ] **Phase 2: Entity Gizmos & Pickups**
  - Implement 3D transform gizmo (Translate/Rotate) for enemy spawns (`chara_spawns.h`).
  - Render `.PLM` 3D prop models for world item pickups (`g_CommonWorldObjectPoses`).

- [ ] **Phase 3: Door Links & Waypoint Editing**
  - Render 3D waypoints (`map_points.h`) and green door trigger boxes (`mapEvents`).
  - Draw directed 3D splines linking door triggers to destination waypoints.

- [ ] **Phase 4: Spatial Room Grid & Environment Controls**
  - Build top-down 2D/3D grid painter for `MAP_ROOM_IDXS` (`Map_RoomIdxGet.h`).
  - Build ImGui Environment Panel for fog distance, lighting tint (`field_16`), weather (`field_17`), and BGM layer volume mixers.

- [ ] **Phase 5: Source Exporter & Asset Repacker Bridge**
  - Implement C header exporter (emits `vc_road_data.h`, `chara_spawns.h`, `map_points.h`, `mapX_sYY_header.c`).
  - Integrate `.IPD` repacker and ROM disc table update tool.
