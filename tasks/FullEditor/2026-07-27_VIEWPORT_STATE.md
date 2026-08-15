# Phase 5 — 3D Viewport: Full Technical State & Roadmap

> Created: 2026-07-27  
> Status: Phase 6 active. Viewport split into functional derivations (View, Edit, Collision). Undo/Redo and global shortcuts are fully operational.

---

## 1. Repository Layout (Relevant to Viewport)

```
tooling/unified_cpp_editor/
  src/
    core/
      ChunkManager.cpp/.h         — 2D grid UI, LMB selection, pipeline actions
      IpdParser.cpp/.h            — Full IPD+PLM geometry parser -> ParsedChunk
      IpdInspector.cpp/.h         — Lightweight inspector (header stats only, no geometry)
      TextureManager.cpp/.h       — TIMImage (TIM loader) + TextureCache (GPU registry)
      TextureManagerWindow.cpp/.h — ImGui panel for texture preview and multi-edit
      SettingsWindow.cpp/.h       — ImGui panel for modifying editor preferences
      ToolPanelWindow.cpp/.h      — ImGui panel for active tool parameters
      ipd_structs.h               — All binary structs: IPD_FILE_HEADER, PLM_*, etc.
    viewport/
      Viewport3DBase.cpp/.h       — Abstract base for 3D camera, rendering, and GPU meshes
      ViewViewport.cpp/.h         — Standard visual rendering of opaque geometry
      EditViewport.cpp/.h         — Interactive structural geometry editing (translate, faces)
      CollisionViewport.cpp/.h    — Specialized rendering for collision boundaries
      SceneOutliner.cpp/.h        — ImGui chunk collection tree panel
    main.cpp                — App entry, docking layout, wiring
  CMakeLists.txt            — Ninja/MinGW build, FetchContent for raylib/imgui/rlImGui
```

---

## 2. Architecture Overview

### 2a. IpdParser — Geometry Pipeline

**Entry point:** `IpdParser::Parse(ipdPath, workspaceDir, out: ParsedChunk)`  
**Then call:** `IpdParser::BuildBatches(chunk)` to flatten geometry for GPU

**Parse flow (faithful port of `4_blender_bridge/json_to_blender.py`):**
1. Read IPD binary ? `IPD_FILE_HEADER` (84 bytes, magic `0x14`)
2. Read `obj_name_table` (array of `IPD_OBJNAME_DATA`, 16 bytes each):
   - `flag = 0` ? object lives in the **embedded local PLM** at `ipd_hdr.plm_offset`
   - `flag = 1` ? object lives in **`{PREFIX}_GLB.PLM`** (separate file)
3. Read `pos_groups` (array of `IPD_POS_HEADER`, 24 bytes each):
   - Each group has `obj_num` entries of `IPD_OBJ_DATA` (36 bytes each)
   - `IPD_OBJ_DATA`: 3x3 rotation matrix (fixed-point /4096) + 3 int32 translations
4. For each placed object: look up `PLM_OBJ_HEADER` by name ? read `PLM_DATA_HEADER` per mesh ? read vertices (XY at `vert_xy_offset`, Z at `vert_z_offset`) ? read packs (`PLM_PACK_HEADER`, 20 bytes)
5. Apply coordinate transform, UV bias, store `RenderFace`

**Coordinate math (from coordinate_math.py):**
- SCALE = 1/256, MAP_MAX = 10240
- Local vertex: x=-px*SCALE, y=-pz*SCALE, z=-py*SCALE
- World translation: x=-(tx + MAP_MAX*xPos)*SCALE, y=-(tz + MAP_MAX*yPos)*SCALE, z=-ty*SCALE
- Rotation permutation for axis swap: rot[0]={r11,r13,r12}, rot[1]={r31,r33,r32}, rot[2]={r21,r23,r22}

**UV bias:** u_final = (u==max_u && max_u>min_u) ? u+1 : u (same for V). Normalise: u_final/tw, 1-v_final/th

**CBA decode:** palette_row = (cba & 0x7FC0) >> 6; tex_num = tex_num_and_unk2_byte & 0x7F; 0x7F = no-texture

**Quad winding:** Quad (faces_3!=0xFF): verts (v0,v2,v3,v1). Triangle: verts (v2,v1,v0)

**Global PLM:** Only loaded if flag=1 objects exist. Path: {workspaceDir}/chunks/{PREFIX}_GLB.PLM. Graceful skip if missing.

---

### 2b. TextureCache

Singleton: `TextureCache::Get()`
- `Fetch(texName, paletteRow, workspaceDir)` ? loads TIM from `{workspaceDir}/textures/{texName}.TIM`
- Caches one `Texture2D` per `(texName, paletteRow)` pair; TEXTURE_FILTER_POINT
- `TIMImage::BuildPaletteTexture(row)` materialises one palette row without touching m_texture
- `UnloadAll()` before CloseWindow()

---

### 2c. Viewport3D

State: Camera3D, RenderTexture2D (dynamic resize), vector<LoadedChunk>, m_hovered, m_showGrid

`LoadChunk(ipdPath, workspaceDir)`:
1. IpdParser::Parse ? BuildBatches
2. BuildGpuBatches: for each RenderBatch ? RL_MALLOC + UploadMesh + LoadMaterialDefault + TextureCache::Fetch

`Draw()`:
1. EnsureRenderTarget (resize when panel changes)
2. UpdateCamera (when hovered)
3. BeginTextureMode ? BeginMode3D ? DrawGrid ? DrawMesh×N ? EndMode3D ? EndTextureMode
4. ImGui::Image with Y-flipped UVs (uv0={0,1}, uv1={1,0})

---

### 2d. ChunkManager

- `m_selectedChunks` (LMB): pipeline actions (extract/deploy) + IPD Inspector + Texture Manager
- `m_viewportChunks` (RMB, PLANNED): 3D viewport auto-sync selection shown in magenta
- `GetSelectedChunks()`: public accessor for LMB selection
- `GetViewportChunks()`: PLANNED public accessor for RMB selection
- `DrawGrid()`: iterates grid, colours by extracted/deployed/selected state, LMB click/drag selection

---

## 3. Known Bugs and Planned Fixes

### Bug 1 + 2 — Camera behaviour broken
**Symptoms:** Grid rotates on any mouse movement over the panel (not just RMB). MMB pan does not work.  
**Root cause:** `::UpdateCamera(&m_camera, CAMERA_THIRD_PERSON)` samples Raylib's global input state on every hovered frame, ignoring whether any button is actually pressed in the ImGui context.  
**Fix:** Replace with manual camera implementation in `Viewport3D::UpdateCamera()`:
- Use `ImGui::GetIO().MouseDelta` (not Raylib GetMouseDelta) to isolate from other panels
- Gate on `ImGui::IsWindowHovered() && !ImGui::GetIO().WantCaptureMouse`
- RMB held + drag ? orbit (azimuth/elevation spherical coordinates)
- MMB held + drag ? pan (translate target + position together in camera's X/Y plane)
- Scroll wheel ? zoom (adjust distance, clamp to [0.5, 1000])
- Store m_azimuth, m_elevation, m_distance as member variables

### Bug 3 — "Load to Viewport" does not work reliably
**Root cause:** Button relies on `pipelineManager.GetWorkspaceDir()` which may be unset. Also, the LMB selection used for pipeline actions conflates "selected for editing" with "selected for viewport" — bad UX.  
**Fix:** Remove the "Load to Viewport" button. Replace with RMB viewport selection (Feature 4).

### Feature 4 — RMB viewport selection (magenta) + auto-sync to 3D viewer
- Add `m_viewportChunks: vector<string>` + `GetViewportChunks()` to `ChunkManager`
- In `DrawGrid()`: RMB click toggles in/out of `m_viewportChunks`
- Colour: magenta `IM_COL32(180,40,180,255)` for viewport-only; bright magenta `IM_COL32(255,100,255,255)` for both LMB+RMB selected
- Legend: add magenta "Viewport" entry
- In `main.cpp`: auto-sync each frame (diff m_viewportChunks vs last frame ? LoadChunk/UnloadChunk)

### Feature 5 — Scene Outliner: flat chunk listing (done)
Already shows: chunk name, visibility toggle, error state, object counts, GPU batch list. Future expansion: per-object tree.

---

## 4. Future Roadmap (Post-Session)

### Phase 6A — Viewport polish
- Backface culling toggle
- Depth-sorted alpha blending for semi-transparent polygons (STP bit ? alpha=127)
- Fog/ambient lighting modes

### Phase 6B — Collision overlay
- Parse IPD_COLL_HEADER from already-loaded buffer
- Render collision triangles as coloured wireframe per surface type
- Toggle per-chunk in Scene Outliner

### Phase 7 — Texture Editor / UV clip
- UV clip region selection per tile group (unclipped TIM pages ? define clip rect)
- Integrates with TextureManager panel

### Phase 8 — Object interaction
- Scene Outliner: per-object tree (tex ? palette ? component, matching Blender 3-tier)
- Ray picking for 3D object selection
- Property editing panel (translate/rotate IPD_OBJ_DATA)

---

## 5. Data Flow

```
ChunkManager 2D grid
  LMB ? m_selectedChunks  ?  IPD Inspector, Texture Manager, pipeline actions
  RMB ? m_viewportChunks  ?  [auto-sync each frame] ? Viewport3D.LoadChunk/UnloadChunk

IpdParser.Parse(ipdPath)
  ? IPD_FILE_HEADER (xPos, yPos, plm_offset, pos_groups)
  ? local PLM (tex_names, objects)
  ? _GLB.PLM (if flag=1 objects)
  ? world matrix per placed object
  ? ParsedChunk { objects: [RenderObject{meshes:[RenderMesh{vertices,faces}]}] }

IpdParser.BuildBatches ? groups by (texNum,paletteRow) ? RenderBatch[]

Viewport3D.BuildGpuBatches
  ? TextureCache::Fetch(texName, paletteRow) ? TIMImage::BuildPaletteTexture ? Texture2D
  ? UploadMesh ? LoadMaterialDefault + MAP_DIFFUSE

Viewport3D.Draw
  ? EnsureRenderTarget ? UpdateCamera (manual) ? BeginTextureMode ? DrawMesh×N ? ImGui::Image
```

---

## 6. Struct Sizes (static_assert verified in main.cpp)

| Struct | Size |
|--------|------|
| IPD_FILE_HEADER | 84 bytes |
| IPD_COLL_HEADER | 308 bytes |
| IPD_OBJNAME_DATA | 16 bytes |
| IPD_POS_HEADER | 24 bytes |
| IPD_OBJ_DATA | 36 bytes |
| PLM_FILE_HEADER | 20 bytes |
| PLM_OBJ_HEADER | 16 bytes |
| PLM_DATA_HEADER | 24 bytes |
| PLM_PACK_HEADER | 20 bytes |
| TIM_FILE_HEADER | 8 bytes |
| TIM_CLUT_HEADER | 12 bytes |
| TIM_IMG_HEADER | 12 bytes |

All use `#pragma pack(push, 1)` in `ipd_structs.h`.

---

## 7. Build System

- Compiler: MinGW GCC 16 (MSYS2) via CMake + Ninja
- Libraries: Raylib (git master), Dear ImGui (docking branch), rlImGui (main)
- `file(GLOB_RECURSE SOURCES "src/*.cpp")` — reconfigure after adding new .cpp files
- Static linking: -static -static-libgcc -static-libstdc++ (MINGW)
- Build: `cmake .. -G Ninja` then `cmake --build .` from `build/`
