# Unified C++ Editor - Changelog

## 2026-08-12 - Core Editor Tweaks
- **[2026-08-12]** Removed stub panels (`SpawnsViewport`, `CameraViewport`, `AudioViewport`, `TextureEditorViewport`) from the main editor layout.

## 2026-08-06 - Missing Geometry & Viewport UI Polish
- Fixed a major chunk prefix parsing bug in `IpdParser.cpp`, `TextureManagerWindow.cpp`, `GlobalShortcuts.cpp`, and `IpdInspectorWindow.cpp` that caused chunks with hex coordinate letters (`A`, `B`, `C`, `D`) like `THRFBFE` and `THRFDFC` to fail global `_GLB.PLM` resolution.
- Added standard `DeriveChunkPrefix` helper in `IpdParser.h` that cleanly strips the 4-digit hex coordinates (`XXZZ`) from standard chunk names (e.g., `THRFBFE` -> `THR`, `SC0102` -> `SC`), resolving missing road components, street lamps, and shared global PLM geometry across negative/hex coordinate chunks.
- Updated `chunk_extractor.py` python script to use the unified hex coordinate stripping logic.
- Updated 3D Viewport Legend to vertically visualize the maximum theoretical chunk bounds (`-16.0f` to `16.0f` world units), while keeping XZ dimensions locked to the 40x40m grid cell boundaries.
- Added a `Reset Camera` button to the status toolbar immediately to the right of `Controls` in `Viewport3DBase`, enabling quick one-click camera resets.

## 2026-08-05 - Geometry Editing & Architecture Refactor
- Split the 3D viewport into functional derivations (`ViewViewport`, `EditViewport`, and `CollisionViewport`) extending from `Viewport3DBase`.
- Implemented a deep-copy `MeshSnapshot` stack in `EditHistory.cpp` for a robust global Undo/Redo buffer.
- Added `PushOrMerge` logic to intelligently group continuous keystroke translations into single discrete Undo actions.
- Implemented `TranslateSelection` in `EditViewport` to allow direct manipulation of mesh vertices.
- Refactored `RebuildChunkBatches` in viewports to utilize `memcmp` against Raylib GPU buffers, enabling fast vertex patching via `UpdateMeshBuffer` without unnecessary VBO reallocation.
- Extracted global shortcuts (Ctrl+Z, Ctrl+Y, Ctrl+S) to `main.cpp` scope, resolving a bug where shortcuts were gated behind specific UI panels.
- Decoupled `IpdWriter::WriteChunk` save logic from requiring an active object name.
- Added `SettingsWindow` for modifying editor preferences, directory paths, and interface settings.
- Added `ToolPanelWindow` for configuring active structural tool parameters.

## 2026-07-31 - Collision Viewport Upgrades
- Fixed rendering logic that caused collision geometry meshes to appear completely black by properly loading default materials.
- Rewrote the IPD wall parser in `BuildCollisionBatches` to parse `s_IpdCollSurface` tilt flags and subcell connectivity correctly.
- Categorized collision walls into types using bitflags: Physical Walls, Camera Blocking Walls, Void Boundaries, and Physical+Void hybrids.
- Visually differentiated each wall type in the viewport (Physical = Red, Camera = Blue, Void = Grey, Mixed = Orange).
- Fixed the extrusion logic for walls: walls now project accurately from the 3D surface height (the top of the wall) down to exactly `Y = 0.0f` on the ground.
- Implemented a "minimum height" clamp for wall extrusions (e.g., 2.5f for physical walls) ensuring they remain visible even if the ground geometry dips below Y=0.
- Extracted and explicitly rendered subcell floor-type boundaries as distinct physical lines in the 3D viewport.
- Decoded the subcell's bordering `s_IpdCollSurface::tilt_flags & 0x1F` to accurately determine the `groundType` (floor sound index) of the subcell edge.
- Generated deterministic visual colors (HSV Hue) based on the `groundType` integer so identical floor sounds are styled uniformly.
- Extensively overhauled the camera system in `Viewport3DBase` to support decoupled 2D Orthographic Projections (Top, Front, Back, Left, Right).
- Synchronized camera positional movement between Perspective and Orthographic modes while deliberately decoupling rotation.
- Overhauled camera controls to follow standard Blender conventions:
  - Keys `4`, `6`, `7`, `8`, `9` instantly swap orthographic viewpoints.
  - Added a hovering tooltip when holding the ``` ` ``` key to display the viewpoint layout.
  - Holding `MMB` (Middle Mouse Button) orbits the camera, instantly snapping out of orthographic views back into perspective.
  - Holding `Shift + MMB` triggers 2D panning along the active view plane.
- Added a `DrawToolbarExtras` hook to `Viewport3DBase`, allowing derived viewports to extend the UI toolbar.
- Used the toolbar hook in `CollisionViewport` to add a `Geometry` toggle checkbox, allowing users to hide the solid map mesh and focus purely on the collision boundaries.
