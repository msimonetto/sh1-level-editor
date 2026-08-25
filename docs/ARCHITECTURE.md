# Silent Hill Level Editor - Architecture & Component Map

This document provides a comprehensive, structured reference for the C++ Level Editor codebase. It is designed to be quickly scannable by AI agents and human developers to pinpoint subsystems, classes, callable methods, and binary data structures.

---

## 1. Project Directory Layout

```
sh1-level-editor/
├── include/                  # C++ Header Files (public interface)
│   ├── core/                 # Engine logic, binary parsers, state managers, config
│   ├── formats/              # Binary format parsers, writers, caches (IPD, PLM, TIM, OBJ)
│   ├── geometry/             # 3D mesh operations, topology editing, constraints validation
│   ├── panels/               # ImGui UI windows, inspectors, mode-specific toolbars
│   └── viewport/             # Raylib 3D viewports, cameras, overlays, renderers
├── src/                      # C++ Source Files (implementation mirrored from include/)
│   ├── core/
│   ├── formats/
│   ├── geometry/
│   ├── panels/
│   ├── viewport/
│   └── main.cpp              # Application entry point, docking layout, main loop
├── libs/                     # Third-Party Dependencies (managed via CMake FetchContent)
│   ├── raylib/               # 3D rendering, windowing, math, and GPU abstractions
│   ├── imgui/                # Immediate-mode GUI library (docking branch)
│   └── rlImGui/              # Raylib <-> ImGui integration layer
├── scripts/                  # Python Asset Pipeline (conversion & extraction engine)
│   ├── backend/              # CLI endpoints invoked natively by C++ FileManager
│   ├── core/                 # Modular low-level format parsers and converters
│   ├── batch/                # Batch processing scripts for full disc extraction
│   └── convert.py            # Unified CLI wrapper for asset conversions
├── data/                     # Editor Data Directories
│   ├── assets/               # Read-only extracted PlayStation disc assets
│   ├── workspace/            # Active project working directory (editable IPD/TIM/PLM)
│   └── temporary/            # Temporary output files for diagnostic logging/search
├── docs/                     # Technical specifications, research facts, format docs
│   ├── formats/              # Byte-level format documentation (IPD, Collision, Overlays, JSON)
│   └── research/             # Verified FACTS.md and active HYPOTHESES.md
└── tasks/                    # Task tracking channels and roadmap documentation
    ├── FullEditor/           # Master task coordination channel for the full editor
    └── [Subsystems]/         # Specialized task channels (Dependencies, GlobalObjects, etc.)
```

---

## 2. Core Engine Module (`include/core/` / `src/core/`)

Responsible for binary format parsing, patch serialization, undo/redo history, asset management, and configuration.

### `FileManager.h`
Central workspace and asset management engine. Coordinates filesystem operations, chunk discovery, multi-selection, viewport reload queues, native asset deployment/extraction pipelines, and OBJ exporting.
- **`ChunkInfo`**: Metadata struct containing `name`, `prefix`, grid coordinates `(x, z)`, and `hasCoords`.
- **`FileManager`**:
  - `SetLogCallback(LogCallback cb)` / `SetProgressCallback(ProgressCallback cb)`: Async UI progress tracking and console logging.
  - `GetSelectedChunks()`, `SetSelectedChunks()`: Tracks active multiselect chunks.
  - `GetSelectedPrefix()`: Returns the currently active prefix (e.g. `"THR"`, `"SC"`, `"SU"`).
  - `GetViewportChunks()`, `SetViewportChunks()`: Chunks actively loaded into the 3D scene.
  - `QueueReloadChunks()`, `ConsumeReloadChunks()`: Thread-safe queue for notifying viewports of file changes.
  - `GetWorkspaceDir()`, `GetAssetsDir()`, `GetOverrideDir()`, `GetBuildDir()`, `GetGameBinSource()`: Path resolution helpers.
  - `ScanAssets()`: Scans filesystem and parses chunk coordinate headers to populate the grid.
  - `ExtractToWorkspace(chunks, completeDir, workspaceDir, projectDir)`: Copies and converts raw disc assets into editable workspace files.
  - `DeployToTarget(chunks, workspaceDir, overrideDir, projectDir)`: Smart deployment that repacks workspace assets into target game directory.
  - `DeleteSelected(targetType, chunks, deleteTextures, workspaceDir, overrideDir, projectDir)`: Safely removes selected chunk files.
  - `ClearEntire(targetType, workspaceDir, overrideDir, projectDir)`: Purges all files from workspace or override directories.
  - `RevertSelected(chunks, revertDependencies, workspaceDir, assetsDir)`: Restores workspace assets from original disc backup.
  - `ExportToOBJ(chunks, workspaceDir, assetsDir, projectDir)`: Native exporter for selected chunks and their dependencies to Wavefront `.obj`, `.mtl`, and baked `.png` textures.
  - `DeployOverlayToDecomp(mapKey)`: Exports waypoint and event overlays to C decomp header files.
  - `Log()`: Dispatches console messages.

### `Config.h`
Singleton class managing persistent user preferences via `config.json`.
- **`Config::Get()`**: Accessor for global configuration instance.
- **Properties**:
  - Project/Game directory paths, active prefix, last loaded map key.
  - Interface colors (`ColorSelected`, `ColorWorkspace`, `ColorDeployment`, `ColorUnloaded`, `WireframeColor`).
  - Viewport & grid options (`ShowMajorGridlines`, `ShowMinorGridlines`, `GridCellSize`, `EnableDitheringMode`, `ShowPersistentWireframe`, `WireframeThickness`).
  - Customizable keyboard controls for selection movement and 3D camera navigation.
  - Session state persistence (`PersistedSelection`, `PersistedViewportChunks`, `PersistedCamAzimuth`, `PersistedCamElevation`, `PersistedCamDistance`, `PersistedCamTarget`, `PersistedToolsTab`).
  - Serialization helpers: `Vector3ToString()`, `StringToVector3()`, `StringListToString()`, `ParseStringList()`.

### `DependencyManager.h`
Maintains bidirectional asset relationship indexes via `dependencies.json` and `dependents.json`.
- **`DependencyManager`**:
  - `Load()`, `Save()`: Loads/writes JSON dependency graphs.
  - `LoadIPDDependencies(prefix, ipdNames)`: Resolves active TIM textures and PLM models for selected chunks.
  - `GetActiveTextures()`, `GetActivePLMs()`: Set accessors for active workspace dependencies.
  - `GetTexturesForChunks(chunkNames)`: Queries required texture dependencies for a specific subset of chunks.
  - `GetSharedFiles(excludeChunks)`: Computes files shared with other chunks outside the exclusion list.
  - `m_dependencies`: Prefix $\rightarrow$ (Asset Name $\rightarrow$ Required Dependencies).
  - `m_dependents`: Prefix $\rightarrow$ (Asset Name $\rightarrow$ Dependent Chunks/Files).

### `ResourceFilter.h`
Reusable multi-scope asset query engine decoupled from disk file I/O and UI logic.
- **`ResourceFilter`**:
  - `GetAssetTextures(fileManager, prefixFilter)`: Discovers all TIM textures from extracted game assets (`assets/BG/`, `assets/TIM/`).
  - `GetWorkspaceTextures(fileManager, prefixFilter)`: Discovers TIM textures residing in the active workspace (`workspace/TIM/`).
  - `GetSelectedChunksTextures(fileManager, depManager, prefixFilter)`: Resolves textures required by all multi-selected chunks.
  - `GetChunkTextures(chunk, prefixFilter)`: Collects local and global texture names for a specific parsed chunk.
  - `GetMeshTextures(mesh, chunk, obj, prefixFilter)`: Collects textures actively mapped to faces on a specific mesh.
  - `ResolveTexturePath(fileManager, texName)`: Locates the physical file path of a texture across workspace and asset directories.

### `Dictionary.h`
Manages human-readable aliases and prefix designations via `dictionary.json`.
- **`Dictionary`**:
  - `Load()`, `Save()`: Reads and updates alias JSON files.
  - `PrefixNames`: Map of raw prefixes (`"THR"`, `"SC"`) to descriptive names (`"Old Silent Hill"`, `"School"`).
  - `ChunkAliases`: Map of raw chunk IDs (`"THR0000"`) to custom labels.

### `FileDialog.h`
OS-native file and directory picker dialogs.
- **`FileDialog`**:
  - `OpenFile(filterDescription, filterExtensions)`: Opens system file picker.
  - `OpenDirectory(title)`: Opens system directory browser.

### `History.h`
Unified transactional undo/redo engine using deep snapshots across 3D meshes and map overlays.
- **`MeshSnapshot`**: Before/after state snapshot of a `RenderMesh` with chunk name, object index, and mesh index.
- **`OverlaySnapshot`**: Before/after state snapshot of an `OverlayMapData` structure.
- **`EditRecord`**: Tagged union holding either a `MeshSnapshot` or an `OverlaySnapshot`.
- **`History`**:
  - `Push(MeshSnapshot snap)` / `Push(OverlaySnapshot snap)`: Pushes a new operation to the undo stack.
  - `Undo(viewport, localGeoOverlay, waypointsOverlay, workspaceDir)`: Reverts the latest change in memory and triggers GPU batch rebuilds.
  - `Redo(viewport, localGeoOverlay, waypointsOverlay, workspaceDir)`: Reapplies the next change.
  - `CanUndo()`, `CanRedo()`, `PeekUndoDesc()`, `PeekRedoDesc()`: Status and description queries for UI menus.
  - `SetMaxDepth()`, `GetMaxDepth()`, `Clear()`: Buffer depth management.
  - `IsEqual()`, `CanMerge()`: Deep comparison and continuous transform merging logic.

### `IPDParse.h`
Low-level parser for PlayStation 1 `.IPD` (world geometry) and standalone `_GLB.PLM` binary files.
- **Coordinate System Constants**: `IPD_SCALE = 1.0f / 256.0f`, `IPD_MAP_MAX = 10240.0f`.
- **`DeriveChunkPrefix(name)`**: Utility to extract prefix from standard 8-character chunk names.
- **`FaceAddress`**: Canonical address of a polygon in binary (`plmObjectName`, `meshIdx`, `packIdx`, `isGlobal`, `packRawOffset`).
- **`RenderFace`**: Decoded polygon primitive containing vertex indices `v[4]`, normalized `uv[4][2]`, `paletteRow`, `texNum`, `texName`, raw UV bytes `rawU[4]`/`rawV[4]`, normal indices, and raw CBA words.
- **`RenderMesh`**: Container for local vertex arrays `(vx, vy, vz)` and constituent `faces`.
- **`RenderObject`**: Placed 3D model instance (`PLM_OBJ_HEADER`) with local meshes, world transform matrix `rt[3][3]`, translation `(rawTx, rawTy, rawTz)`, bounding box, and `.IPD` file offset.
- **`RenderBatch`**: Geometry grouped by `(texName, paletteRow)` with interleaved positions and UVs for fast GPU rendering.
- **`ParsedCollision`**: Extracted physics collision data (`splitVertices`, `surfaces`, `subcells`, `grid`, and indirection tables).
- **`ParsedChunk`**: Top-level chunk structure containing collision, local/global texture names, placed objects, and render batches.
- **`IPDParse`**:
  - `Parse(ipdPath, workspaceDir, outChunk)`: Parses complete `.IPD` file and associated `_GLB.PLM`.
  - `BuildBatches(chunk)`: Flattens object geometry into GPU-ready draw batches.
### `GlobalCache.h`
Thread-safe in-memory cache for PlayStation 1 `_GLB.PLM` binary asset libraries.
- **`CachedGlobal`**: In-memory representation of a loaded global PLM library (`path`, `buffer`, `globalTexNames`, `globalObjMap`).
- **`GlobalCache`**: Singleton registry caching `_GLB.PLM` buffers in RAM to eliminate redundant disk I/O and duplicate parsing across chunks.
  - `GetOrLoad(glbPath)`: Fetches cached global PLM or reads from disk once.
  - `Invalidate(glbPath)`: Evicts modified file from cache on save.
  - `Clear()`: Flushes all cached GLB files on workspace lifecycle operations.

### `IPDWrite.h`
Intelligent section-aware binary patch writer that writes in-memory geometry edits back to `.IPD` and `.PLM` files.
- **`IPDWrite`**:
  - `WriteChunk(ipdPath, glbPath, chunk, outPatchedCount, outFilesWritten)`: Re-encodes modified vertices/faces, shifts binary sections, updates file header offsets, and writes back atomically.
  - `Validate(chunk)`: Pre-save structural integrity validation.
  - `UpdateMeshStructure(buf, chunk, isGlobalFile)`: Dynamically resizes PLM data buffers when vertex or face counts change and recalculates pointers.
  - `EncodeFaceAtOffset()`, `EncodeUVs()`: Reverses UV normalization and packs bytes into PS1 bitfields.
  - `RelocateIPDOffsets()`, `RelocatePLMOffsets()`: Shifts binary table pointers across section insertions/deletions.
  - `CalculateSHA256()`, `WriteFileAtomic()`: Safe disk writing with integrity checking.

### `MapTable.h`
Registry table mapping 43 decomp-verified Silent Hill map sections.
- **`MapInfoEntry`**: Structure containing `key` (e.g. `"MAP0_S00"`), `prefix` (e.g. `"THR"`), and `description`.
- **`MAP_REGISTRY_TABLE`**: Complete constant lookup array of all 43 game map overlays.

### `OBJExport.h`
Wavefront `.OBJ` geometry exporter.
- **`OBJExport::ExportChunk(chunk, outObjPath, workspaceDir, assetsDir, exportCollision)`**: Converts parsed chunk geometry into standard `.obj` mesh, `.mtl` material definitions, and baked `.png` textures.
- **`OBJExport::Export(inspector, depMgr, outputPath, exportCollision)`**: Generic export interface.

### `OverlayLoader.h`
Parses and serializes map director overlay data (`map_points.json` and `events.json`).
- **`WaypointData`**: Arrival coordinates `(worldX, worldZ)`, orientation angle, trigger dimensions, paper map ID, and loading screen ID.
- **`LinkData`**: Door trigger metadata, destination map index, trigger activation types, system states (`SysState_LoadRoom`, `SysState_LoadOverlay`), and item requirements.
- **`OverlayMapData`**: Aggregates all waypoints and door links for a given map section.
- **`OverlayLoader`**:
  - `Load(mapKey, outData)`: Reads JSON files from `data/workspace/overlays/<MAP_KEY>/`.
  - `Save(mapKey, data)`: Serializes edited waypoints and links back to JSON.
  - `GetMapKeyForChunk(chunkName)`: Helper mapping chunk prefixes to primary map overlay keys.

### `Patcher.h`
Executable patcher for PlayStation executable binaries (e.g. `SLUS_007.07`).
- **`EnginePatcher`**:
  - `PatchMemoryAllocations(exePath, version)`: Increases game heap and resource allocation pools.
  - `RevertMemoryAllocations(exePath, version)`: Restores vanilla binary bytes.
  - `CheckPatchingRequired(exePath, version)`: Verifies patch status.

### `PLMParse.h`
Parser for PlayStation 1 `.PLM` models and standalone `_GLB.PLM` binary asset libraries.
- **`PLMParse`**:
  - `ParseAndPlaceObject(...)`: Parses individual PLM object headers and applies 3D fixed-point world transform matrices.
  - `ParseGlbFile(glbPath, outObjects, outTexNames, outInfo)`: Standalone parser for global PLM asset libraries.

### `Shortcuts.h`
Global keyboard shortcut router.
- **`Shortcuts`**:
  - `Handle(history, fileMgr, viewport, localGeoOverlay, waypointsOverlay)`: Dispatches keyboard hotkeys (Undo, Redo, Save, Deselect, Camera Focus, Mode switching).
  - `SaveSelected()`, `SaveAll()`: Triggers patch writer pipeline for active workspace assets.

### `Structs.h`
Header-only definitions of packed PlayStation 1 C-style binary structs (`#pragma pack(push, 1)`):
- File Headers: `IPD_FILE_HEADER`, `PLM_FILE_HEADER`, `TIM_FILE_HEADER`.
- Section Headers: `PLM_OBJ_HEADER`, `PLM_DATA_HEADER`, `PLM_PACK_HEADER`, `IPD_OBJ_DATA`, `TIM_IMG_HEADER`, `TIM_CLUT_HEADER`.
- Collision Structures: `IPD_COLL_HEADER`, `IPD_COLL_SVECTOR`, `IPD_COLL_SURFACE`, `IPD_COLL_SUBCELL`.
- Director / Event Structures: `MapPoint2d`, `EventData`.

### `TextureCache.h`
Thread-safe in-memory GPU texture cache and registry for the 3D viewport.
- **`TextureCache`**:
  - Singleton caching Raylib GPU `Texture2D` instances for all loaded workspace textures to prevent redundant VRAM allocations.
  - `Fetch(texName, paletteRow, workspaceDir)`: Retrieves or loads texture from disk.
  - `Preload(texName, workspaceDir)`: Worker-thread friendly background loading.
  - `GetDimensions(texName, workspaceDir, w, h)`: Retrieves texture dimensions.
  - `GetAlphaCutoutShader()`: Returns or compiles the shared GLSL 330 alpha-cutout shader with discard support.
  - `CreateMeshMaterial(texName, paletteRow, workspaceDir)`: Builds a Raylib `Material` with the alpha-cutout shader and diffuse texture.
  - `UnloadAll()`: Cleans up GPU memory and shaders on exit or reload.

### `Textures.h`
PlayStation `.TIM` texture loader and CLUT palette manager.
- **`TIMPalette`**: Represents 16-color or 256-color palette data.
- **`Textures`**:
  - `Load(path)`, `Unload()`: Loads raw `.TIM` images.
  - `SaveToPNG()`, `LoadFromPNG()`: Converts TIM to PNG and vice versa.
  - `BuildPaletteTexture()`: Generates GPU texture using selected CLUT row.


---

## 3. UI Panels Module (`include/panels/` / `src/panels/`)

Modular ImGui windows providing specialized editing controls and inspectors.

### `ChunksPanel.h`
- **`ChunksPanel`**: Renders the 2D chunk grid overview, prefix selector, deployment/extraction action buttons, interactive status console, and pipeline progress modals.

### `DependenciesPanel.h`
- **`DependenciesPanel`**: Workspace file manager. Provides asset library browsing, dependency inspection, and safe asset import/removal.

### `LocalGeometryPanel.h`
- **`LocalGeometryPanel`**: Blender-inspired 3D modeling tool panel. Organizes tools across 4 modes:
  1. *Global Objects*: Instantiated `_GLB.PLM` props, yaw rotation, grid/floor snapping, duplicate, mirror, delete.
  2. *Meshes*: Whole-mesh translation, 90° axis rotation, axis mirroring, floor snapping, mesh merging/separation, primitive spawning (planes, cubes, slopes), pivot centering, chunk migration.
  3. *Faces*: Quad triangulation, triangle bridging, normal extrusion/detachment, normal inversion, face deletion with isolated vertex cleanup, 3D tile painting, UV rotation/flipping/fitting/reset.
  4. *Vertices*: Power-of-two grid snapping, floor snapping, XYZ planarization, new face creation from 3/4 points, vertex/edge extrusion, tolerance welding, vertex deletion.
  - *Validation*: Runs `ChunkValidator` on demand and renders interactive warnings/errors with jump-to-element buttons.

### `MapsPanel.h`
- **`MapsPanel`**: Map registry browser listing all 43 game map sections with search filtering, overlay loading triggers, and C decomp export actions.

### `MenuPanel.h`
- **`MenuPanel`**: Top application menu bar. Routes File operations (Save, Revert, Open Directories, Quit), Edit operations (Undo/Redo history), Panels visibility toggles, Viewport controls, and Help documentation.

### `OutlinerPanel.h`
- **`OutlinerPanel`**: Scene hierarchy treeview showing loaded chunks, objects, and meshes with visibility checkboxes, alias labels, and selection sync.

### `SettingsPanel.h`
- **`SettingsPanel`**: Editor preferences panel for editing paths, interface theme colors, grid dimensions, wireframe settings, keybindings, and undo buffer depth.

### `TextureEditPanel.h`
- **`TextureEditPanel`**: Standalone pop-out texture editor window for TIM texture inspection and modification. Operates on an isolated in-memory working copy of `DecodedTIM`, provides zoomable pixel canvas navigation with pixel grid and hover inspection, CLUT palette table editing with 15-bit BGR555 quantization and STP flag toggling, and atomic Save/Revert synchronization with `TextureCache`, `TextureMapPanel`, and 3D viewport batch rebuilds.

### `TextureMapPanel.h`
- **`SelectedTile`**: Bounding box representing active UV tile selection.
- **`TextureMapPanel`**: Texture and UV mapping panel. Displays loaded TIM textures, CLUT row selector with interactive color bar, 2D UV canvas with real-time LMB box and MMB single-vertex coordinate dragging, active dependency tracking via `DependencyManager`, tile grid picker, and controls for 3D face paint mode.

### `TextureWidgets.h`
- **`TextureSelectorWidget`**: Reusable texture discovery and selection widget with scope filtering (`Assets`, `Workspace`, `Selected Chunks`, `Current Chunk`, `Current Mesh`), combo dropdown, and "From file" browser.
- **`PaletteInspectorWidget`**: Reusable CLUT palette widget with row stepper buttons, dropdown combo with 16-color preview strips, and interactive hoverable color swatch palette bar.

### `ViewportToolsPanel.h`
- **`ViewportMode`**: Enum defining active editor viewport modes (`Scene`, `LocalGeometry`, `Collision`, `DoorsAndWaypoints`, `Spawns`, `Camera`, `Audio`, `TextureEditor`).
- **`ViewportToolsPanel`**: Tabbed conduit panel hosting mode selectors and delegating content drawing to active mode inspectors (`LocalGeometryPanel`, `WaypointsPanel`, etc.).

### `WaypointsPanel.h`
- **`WaypointsPanel`**: Inspector for door triggers and navigation waypoints. Provides controls for arrival angles, trigger volumes (AABB, Facing, OBB), destination maps, system states, and door linking.

---

## 4. Viewport Module (`include/viewport/` / `src/viewport/`)

Raylib-powered 3D rendering pipeline, camera systems, and modular viewport overlays.

### `ViewportBase.h`
Abstract foundation for 3D viewports.
- **`GpuBatch`**: Raylib mesh and material wrapper holding GPU buffers.
- **`LoadedChunk`**: Associates parsed chunk geometry with its uploaded GPU batches, bounding boxes, and visibility state.
- **`ViewportCameraState`**: Encapsulates camera target, distance, elevation, azimuth, and projection mode (Perspective / Orthographic).
- **`ViewportBase`**:
  - Manages Raylib `RenderTexture2D` canvas, ImGui window image blitting, mouse orbit/pan/zoom, and camera state persistence.
  - Renders major/minor ground grids and chunk boundaries.
  - Provides virtual hooks for `DrawScene()`, `HandlePicking()`, `HandleBoxPicking()`, `DrawContextMenu()`, and lifecycle management.

### `ViewportOverlay.h`
Interface for modular viewport overlays.
- Overlays implement `LoadChunk()`, `UnloadChunk()`, `UnloadAll()`, `DrawSceneOverlay()`, `Draw2DOverlay()`, `DrawContextMenu()`, and picking handlers.

### `Viewport.h`
The unified visual geometry renderer and host for viewport overlays.
- **`Viewport`** (inherits `ViewportBase`):
  - Hosts active `LoadedChunk` geometry.
  - Dispatches rendering and picking events to registered overlays based on `m_activeMode`.
  - Rebuilds GPU batches incrementally on geometry changes via `RebuildChunkBatches()`.

### `SceneOverlay.h`
- **`SceneOverlay`**: Standard read-only scene layout viewer.

### `CollisionOverlay.h`
- **`CollisionOverlay`**: Visualizes walkable 2.5D floor terrain, sloped surfaces, and vertical wall occlusion planes parsed from `.IPD` collision headers.

### `Frustum.h`
- **`Frustum`**: Extracts camera view frustum planes (`FromCamera()`) and performs fast sphere/box culling to optimize rendering of large multi-chunk scenes.

### `GlobalGeometryViewport.h`
- **`GlobalGeometryViewport`**: Standalone 3D viewport for browsing and inspecting global `_GLB.PLM` props with isolated camera controls.

### `LocalGeometryOverlay.h`
- **`EditMode`**: Enum (`GlobalObject`, `Mesh`, `Face`, `Vertex`).
- **`SelectedVertex`**, **`SelectedFace`**: Structures tracking multiselect items with selection order.
- **`LocalGeometryOverlay`**: The central interactive 3D editing layer. Handles raycast picking, box selection, transform gizmo manipulation, vertex/face highlighting, and wireframe overlays.

### `ViewportSync.h`
- **`ViewportSync`**: Coordinates asynchronous background chunk loading and synchronizes loaded state between `FileManager`, `Viewport`, and `LocalGeometryOverlay`.

### `WaypointsOverlay.h`
- **`WaypointsOverlay`**: Renders 3D waypoint pins, orientation arrows, color-coded door trigger volumes, and spline curves linking connected rooms.

### `Wireframe.h`
- **`Wireframe` Namespace**: Shared rendering routines for persistent wireframes, selected face outlines, and vertex point overlays.

---

## 5. Geometry & Constraints Module (`include/geometry/` / `src/geometry/`)

Low-level polygonal math, structural operations, and PlayStation hardware constraints enforcement.

### `ChunkValidator.h`
Enforces PlayStation hardware constraints and `.IPD` layout rules:
- **Constraints**: 1 world unit = 256 raw units, grid cell = 40.0 world units (10240 raw units), max height = 16.0 world units (4096 raw units), max overhang = 16 raw units, max vertices per mesh = 255 (PS1 uint8 index limit).
- **`ChunkValidator`**:
  - `ValidateChunk(chunk)`: Validates entire chunk for vertex overflows, boundary violations, and height limits.
  - `ValidateMesh(mesh, obj, chunk, objIdx, meshIdx)`: Validates single mesh within context.
  - `ValidateVertexPosition(worldPos, xPos, yPos)`: Bounds checks for single points.
  - `DetermineChunkGridPos()`, `DetermineChunkOwnerForMesh()`: Computes spatial ownership.

### `GlobalObjectOperations.h`
Centralized operations for global `_GLB.PLM` prop instances in the `Geometry` namespace:
- `RotateGlobalObject()`: 90° CW/CCW/180° yaw rotation with fixed-point matrix transformation.
- `SnapGlobalObjectToFloor()`, `SnapGlobalObjectToGrid()`: Floor raycasting and grid alignment.
- `DuplicateGlobalObject()`, `DeleteGlobalObject()`, `MirrorGlobalObject()`, `MoveGlobalObjectToChunk()`.

### `MeshOperations.h`
Operations for local chunk meshes and object containers in the `Geometry` namespace:
- `TranslateSelection()`: Unified translation for vertices, faces, and meshes with transactional undo and bounds checking.
- `RotateMesh()`, `MirrorMesh()`: 90° axis rotation and axis mirroring with winding inversion.
- `SnapMeshToFloor()`, `DuplicateMesh()`, `SeparateMeshToNewObject()`, `MergeMeshes()`, `DeleteMesh()`.
- `RecalculateBounds()`, `CenterMeshPivot()`, `AddPrimitive()`, `MoveMeshToChunk()`.

### `FaceOperations.h`
Polygon topology, extrusion, winding, and UV mapping in the `Geometry` namespace:
- `TriangulateFaces()`, `ConnectBridgeFaces()`.
- `ExtrudeFaces()`: Normal extrusion in Connect or Separate mode.
- `InvertNormals()`, `DeleteFaces()`.
- `PaintFaces()`, `ClearTexture()`: 3D texture tile application and clearing.
- `RotateUV()`, `FlipUV()`, `FitUVToTileBounds()`, `ResetDefaultUV()`.

### `VertexOperations.h`
Point-level coordinates, topology generation, and welding in the `Geometry` namespace:
- `SnapVerticesToGrid()`, `SnapVerticesToFloor()`, `PlanarizeVertices()`.
- `AddFaceFromSelectedVertices()`: Builds triangles (3 points) or sorted quads (4 points).
- `ExtrudeSelectedVertices()`, `WeldVertices()`, `DeleteSelectedVertices()`.

### `SubdivideFace.h`
- `SubdivideSelectedFaces()`: Subdivides selected polygon faces into grid-aligned tiles while enforcing the 255-vertex budget.

---

## 6. Python Asset Pipeline (`scripts/`)

The Python conversion bridge converts proprietary PS1 binary formats into standard workstation formats and vice versa.

- **`scripts/convert.py`**: Main CLI dispatcher supporting commands for `tim-to-png`, `png-to-tim`, `ipd-to-json`, `json-to-ipd`, `plm-to-json`, `json-to-plm`, `json-to-obj`, and `inspect`.
- **`scripts/backend/`**:
  - `chunk_extractor.py`: Extracts chunk `.IPD`, `.TIM`, and `_GLB.PLM` files from game assets into `data/workspace/`.
  - `deploy_workspace.py`: Repacks workspace assets into the target game directory.
  - `manage_workspace.py`: Workspace deletion and clean-up routines.
  - `pack_overlay_to_decomp.py`: Exports waypoint and event JSON files to decomp-compatible C source headers.
- **`scripts/core/`**:
  - `ipd_parser.py`: Low-level binary reader/writer for `.IPD` headers, chunks, and collision.
  - `plm_parser.py`: Binary parser/serializer for `.PLM` models.
  - `tim_to_png.py` / `png_to_tim.py`: PlayStation `.TIM` image and 16/256-color CLUT converter.
  - `json_to_obj.py`: Converts intermediate JSON chunk representations to Wavefront `.obj` models.
  - `deps.py`: Dependency graph builder generating `dependencies.json`.

---

## 7. Architectural Rules & Data Flow

### 1. Integer Quantization & Floating Point Scale
- The editor renders and manipulates geometry using floating-point world coordinates ($1\text{ unit} = 256\text{ raw units}$).
- Upon save (`IPDWrite`), coordinates are strictly quantized to native PlayStation signed 16-bit integers (`int16_t`).
- Geometry transformations ensure integer-aligned boundaries to prevent visual micro-cracks and tearing in the PS1 rasterizer.

### 2. Viewport Overlay Dispatch Architecture
- The main `Viewport` window serves as a unified 3D canvas hosting multiple specialized overlays (`LocalGeometryOverlay`, `CollisionOverlay`, `WaypointsOverlay`).
- Active viewport tools in `ViewportToolsPanel` switch the active mode, which re-routes drawing, context menus, and raycast picking to the corresponding overlay without recreating GPU resources.

### 3. Transactional Deep-Copy History
- Edits to 3D meshes or map overlays capture complete deep-copy snapshots (`MeshSnapshot` / `OverlaySnapshot`).
- Undo/Redo operations apply snapshots directly to in-memory data structures and trigger fast GPU batch re-uploads via `RebuildChunkBatches()`.
- Sequential micro-edits (e.g. continuous arrow key translation) are automatically merged to maintain a clean undo history.

### 4. Direct Binary Patching & Section Relocation
- Mesh modifications preserve binary header integrity by recalculating internal pointer tables (`PLM_OBJ_HEADER`, `PLM_DATA_HEADER`, `IPD_FILE_HEADER`).
- Saves are atomic: modifications are written to a temporary buffer, validated, written to a `.tmp` file, and renamed to ensure workspace data cannot be corrupted by interrupted writes.
