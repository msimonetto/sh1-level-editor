# SESSION 2 — PLM Object Manager: Architecture & Implementation Plan

**Goal:** Design and implement an integrated PLM Object Manager for the Unified C++ Editor that allows
browsing, editing, creating, and removing objects in a `_GLB.PLM` file ("the global
asset bank"), viewing which IPD chunks depend on each PLM object, and verifying
PLM file-size constraints — all without breaking IPD roundtrip integrity.

---

## 0. Context Recap

### The Two-Tier Architecture (Established)

| File | Role in the pipeline |
|---|---|
| `[PREFIX]_GLB.json` | Parsed global PLM data — all geometry/textures shared across all chunks for one map prefix (e.g. `THR`). |
| `[CHUNK].json` | Per-IPD chunk data — placement transforms, `glb_flag`, `mesh_id`, but **no geometry**. |
| `_GLB.PLM` (binary) | Rebuilt from `_GLB.json` by the Asset Pipeline (or equivalent). |

An `IPD_OBJ_DATA` record with `glb_flag=1` contains a `mesh_id` that is an index
into the `obj_headers` array inside the `_GLB.PLM` / `_GLB.json`. The IPD itself is
**immutable with respect to global geometry**; only transforms live there. This is
the central design constraint that makes this tool safe to implement.

### What Currently Happens in the Unified Editor

When an IPD is loaded via `IpdParser::Parse()` and visualized in `Viewport3D`:
1. **Global objects** (`glb_flag=1`) are resolved against their corresponding `[PREFIX]_GLB.PLM` (or its parsed output).
2. **Local objects** (`glb_flag=0`) are loaded from the embedded `ipd_plm` data.
3. The renderer batches these meshes, but there is currently no dedicated interface for
   editing the global geometry or managing the `_GLB.PLM` asset bank.

---

## 1. Core Design Principle: Global vs. Local Editing Modes

The requirement: **global objects are a shared asset bank; their geometry/textures/UVs
must not be editable from within a chunk editing context.** Only transforms are chunk-specific.

> **All global PLM geometry objects in the `Viewport3D` are
> permanently placed in a non-editable state for vertex manipulations.** The PLM Manager
> will be a separate ImGui window/context that loads and edits `_GLB.PLM` (or its parsed JSON)
> directly, writes back, and regenerates the `_GLB.PLM` binary.

This is cleaner than trying to mix both modes in the same viewport interaction model.

---

## 2. Proposed Two-Window Workflow

```
+---------------------------------------------+
|  WINDOW A: Viewport3D / IPD Inspector       |
|  Loads: [CHUNK].IPD (via AssetPipeline)     |
|  Editable: transforms of glb_flag=1 objects |
|  Read-only: geometry mesh data              |
|  Export: chunk JSON -> IPD (roundtrip safe) |
+-------------------+-------------------------+
                    | reads shared _GLB.json
                    v
+---------------------------------------------+
|  WINDOW B: PLM Manager (ImGui Window)       |
|  Loads: [PREFIX]_GLB.json / .PLM            |
|  Editable: geometry, UVs, textures of all   |
|            global PLM objects               |
|  Panel: PLM Object List, Dependent Chunks,  |
|         Budget Meter, Add/Remove            |
|  Export: GLB.json -> _GLB.PLM (binary)      |
+---------------------------------------------+
```

Both windows can interact within the same application process, but conceptually operate on
separate data representations (the chunk vs. the global asset bank).

---

## 3. Window A: Enforcing Read-Only Global Geometry

### 3.1 Mechanism

When a chunk is loaded into `Viewport3D`, objects with `glb_flag == 1` are flagged.
If the user attempts to enter Vertex/Face editing mode (if implemented) or tries to
modify the mesh structure, the action is blocked.

- A simple `if (selectedObject->IsGlobal) return;` check in the geometry modification tools.
- Visual feedback (e.g., drawing global objects with a slightly different wireframe color
  or an overlay) indicating they are shared assets.

### 3.2 Transform-Only Editing for Global Objects

Global objects retain `location`, `rotation`, and `scale` as fully editable.
The `ObjExporter` and JSON serializer will only save these transform properties back to the
`IPD_OBJ_DATA` structures. **No changes to the chunk export path are needed.**

---

## 4. Window B: The PLM Manager — Full Specification

### 4.1 File Loading

The PLM Manager ImGui window loads a `_GLB.json` or `_GLB.PLM` file.

**Import flow:**
```
User picks [PREFIX]_GLB.PLM (or its parsed JSON)
    -> Pipeline parses PLM file headers, objects, and textures
    -> Populates PLM Manager state (list of objects, meshes, textures)
    -> Scan companion chunks to build Dependency Index (see sec 5)
```

### 4.2 The PLM Registry (Internal State)

```cpp
struct PLMObjectItem {
    std::string plm_name;        // e.g. "THR1702"
    int mesh_id;                 // index in obj_headers array
    std::vector<std::string> tex_names; 
    bool is_dirty;               // True if edited since last export
    std::string dependent_chunks;// JSON-serialized list or vector of chunk refs
    int vertex_count;
    int pack_count;
    int estimated_bytes;         // live size estimate
};
```

This state is maintained by a `PlmManager` class or similar in the `core/` directory.

### 4.3 The ImGui Window: "PLM Manager"

#### Panel 1: PLM Object List

```
+==================================+
|  PLM Manager                     |
+==================================+
|  Loaded: THR_GLB.PLM   [Browse]  |
+==================================+
|  +----------------------------+  |
|  | * THR1702  [dirty] 48 pk  |  |
|  |   THR1800  124 pk          |  |  <- ImGui::ListBox or Table
|  |   FLOR0000  36 pk          |  |
|  |   ...                      |  |
|  +----------------------------+  |
|  [+]  [-]  [Duplicate]  [Sort]  |
+==================================+
```

- Dirty items show `[*]` prefix or special color.
- Selecting an item updates the Details panel and potentially isolates the object in a preview viewport.
- `[+]` Add: prompts for name, spawns empty mesh, assigns next `mesh_id`.
- `[-]` Remove: shows dependency warning dialog before deleting.
- `[Duplicate]`: clones geometry + entry under a new name.

#### Panel 2: Selected Object Details

```
+==================================+
|  Active: THR1702                 |
|  mesh_id: 42   Verts: 128        |
|  Packs: 48   Est. size: 1.2 KB   |
|                                  |
|  Textures (tex_names array)      |
|  +------------------------------+ |
|  | [0] THR0001F  [View] [Edit] | |
|  | [1] THR0002F  [View] [Edit] | |
|  +------------------------------+ |
|  [+ Add Texture Slot]            |
+==================================+
```

Texture slots map 1:1 to the `tex_names` vector. Adding a slot appends to it.
The 7-bit `tex_num` caps the list at 128 entries; UI disables "Add Texture Slot"
at that limit with a descriptive tooltip (using `ImGui::IsItemHovered()`).

#### Panel 3: Dependent Chunks

```
+==================================+
|  Dependent Chunks (THR1702)      |
|  +------------------------------+ |
|  | THR0000  -- 3 instances     | |
|  | THR0001  -- 1 instance      | |
|  | THR0002  -- 2 instances     | |
|  +------------------------------+ |
|  [Open THR0000 in Viewport]      |
+==================================+
```

Populated from the Dependency Index (sec 5). The "Open in Viewport" button
commands the `ChunkManager` or `Viewport3D` to load that chunk.

#### Panel 4: Budget & Export

```
+==================================+
|  PLM Budget                      |
|  [========--------]  68.4 KB     |
|  [Verify]  [Export _GLB.PLM]     |
|                                  |
|  Status: All objects valid       |
+==================================+
```

- Budget bar: `ImGui::ProgressBar()`.
- "Verify": dry-run serialization for exact byte sizes + all validation warnings.
- "Export": writes `_GLB.PLM` directly or via JSON, then runs Chunk Patcher.

---

## 5. The Dependency Index

### 5.1 Construction

At load time, the editor scans all chunks in the workspace. While the final implementation
will be in C++ within the `ChunkManager` or `AssetPipeline`, we will retain the following Python
script as a prototype/scaffold:

```python
def build_dependency_index(glb_json_path: Path) -> dict[str, list[dict]]:
    """
    Returns: { plm_obj_name: [{"chunk": "THR0000", "instances": 3}, ...] }
    """
    prefix = glb_json_path.stem.replace("_GLB", "")  # e.g. "THR"
    generated_dir = glb_json_path.parent
    deps = {}

    for local_json in sorted(generated_dir.glob(f"{prefix}*.json")):
        with open(local_json) as f:
            data = json.load(f)
        chunk_name = Path(data["source_file"]).stem
        name_table = data["obj_name_table"]
        instance_counts = {}

        for pg in data["pos_groups"]:
            for obj_entry in pg["obj_data"]:
                name_entry = name_table[obj_entry["obj_id"]]
                if name_entry["flag"] == 1:
                    ref = name_entry.get("global_ref") or name_entry["name"]
                    instance_counts[ref] = instance_counts.get(ref, 0) + 1

        for obj_name, count in instance_counts.items():
            deps.setdefault(obj_name, []).append(
                {"chunk": chunk_name, "instances": count}
            )

    return deps
```

In the C++ editor, this logic runs once at load time (O(n_chunks) file reads) and stores
results in `PLMObjectItem::dependent_chunks`. Re-run via a "Refresh Dependencies" button.

---

## 6. PLM File Size — Constraints & Budget System

### 6.1 Is There a Hard Limit?

**No single hardcoded byte limit exists**, but the PS1's 2 MB RAM imposes a runtime
ceiling. In practice, observed `_GLB.PLM` files are **30–120 KB**. The constraint is
the total sum of `_GLB.PLM + all loaded TIM VRAM + IPD section sizes`.

### 6.2 Tiered Budget System

| Tier | Condition | Display |
|---|---|---|
| Green  | < 80 KB | Safe |
| Yellow | 80–120 KB | Approaching limit |
| Red    | > 120 KB | Exceeds observed max |
| Future | Total level RAM (PLM + TIM + IPD) | Not yet implemented |

### 6.3 Fast Size Estimator

The following Python prototype demonstrates the estimation logic. In the Unified C++ Editor, this will be ported to a C++ calculation method on the PLM object state:

```python
PLM_FILE_HEADER_SIZE = 20
PLM_OBJ_HEADER_SIZE  = 20
PLM_DATA_HEADER_SIZE = 16
PLM_PACK_HEADER_SIZE = 16
PLM_VERTEX_XY_SIZE   = 4   # int16 x2
PLM_VERTEX_Z_SIZE    = 2   # int16
TEX_NAME_ENTRY_SIZE  = 24  # 8-char padded to 24

def _estimate_plm_size(registry_items) -> int:
    total = PLM_FILE_HEADER_SIZE
    for item in registry_items:
        total += TEX_NAME_ENTRY_SIZE * len(json.loads(item.tex_names))
        total += PLM_OBJ_HEADER_SIZE
        obj = bpy.data.objects.get(f"PLM_{item.plm_name}")
        if obj and obj.type == 'MESH':
            m = obj.data
            total += PLM_DATA_HEADER_SIZE
            total += len(m.vertices) * (PLM_VERTEX_XY_SIZE + PLM_VERTEX_Z_SIZE)
            total += len(m.polygons) * PLM_PACK_HEADER_SIZE
        total += 3  # worst-case 4-byte alignment padding
    return total
```

Accurate size requires the dry-run "Verify" operator (full C++ serializer call).

---

## 7. Add / Remove / Edit Operations

### 7.1 Add New PLM Object

1. User clicks `[+]` in the ImGui PLM Manager.
2. ImGui dialog prompts for a **name** (max 8 chars, ASCII, no duplicate check).
3. New `PLMObjectItem` appended with `mesh_id = max(existing_mesh_ids) + 1`.
4. Empty mesh structure created in memory, available in the PLM Manager context.
5. Committed on next "Export".

> PS1 `PLM_FILE_HEADER.obj_num` is uint16 (max 65,535). Observed files have < 200.

### 7.2 Remove PLM Object

1. User clicks `[-]`.
2. ImGui confirmation dialog lists all dependent chunks and instance counts.
3. If confirmed:
   - `PLMObjectItem` removed from memory.
   - Orphan report logged to the editor console.
4. `_GLB.json` and `_GLB.PLM` are NOT updated until "Export" is pressed.

> **Critical:** Removing an object shifts all subsequent `mesh_id` indices. The
> Chunk Patcher (sec 8.1) fixes this automatically on export.

### 7.3 Edit Geometry

Users edit the mesh vertices using the `Viewport3D` tools (once implemented for PLM editing mode). The `is_dirty` flag is set when any modification occurs. The editing workflow mirrors the existing local chunk geometry workflow, but explicitly operates on the shared global asset.

---

## 8. The `mesh_id` Reordering Problem

### The Problem

In the binary `_GLB.PLM`, objects are stored in a flat array. `IPD_OBJ_DATA.mesh_id`
is a positional index. Deleting object at index 5 shifts every object above it
down by one, silently corrupting all IPDs that referenced those higher indices.

### The Solution: Stable Name-Based Reference

1. `_GLB.json` identifies every object by **string name** (e.g. `"THR1702"`), not
   by array index.
2. The `.json` files already use `"global_ref": "THR1702"` (a name) in their
   `obj_name_table`. This is looked up by name in the pipeline — NOT a positional index.
3. When `_GLB.PLM` is regenerated, a **name-to-mesh_id remapping table** is produced:
   `{ "THR1702": 42, "FLOR0000": 0, ... }`.
4. The **Chunk Patcher** then auto-updates `mesh_id` in all affected `.json`
   files before any IPD binary rebuild.

### 8.1 The Chunk Patcher

The following Python script acts as the prototype for the chunk patching logic. The final C++ implementation will be integrated into the `AssetPipeline`'s export process.

```python
def patch_chunk_mesh_ids(
    generated_dir: Path,
    prefix: str,
    name_to_new_mesh_id: dict[str, int]
) -> list[str]:
    """
    Scans all [PREFIX]*.json files. For every IPD_OBJ_DATA with glb_flag=1,
    looks up its global_ref in name_to_new_mesh_id and updates mesh_id.
    Returns list of patched chunk stems.
    Does NOT write the IPD binary -- only updates the JSON.
    """
    patched = []
    for local_json in sorted(generated_dir.glob(f"{prefix}*.json")):
        with open(local_json) as f:
            data = json.load(f)
        changed = False
        name_table = data["obj_name_table"]
        for pg in data["pos_groups"]:
            for obj_entry in pg["obj_data"]:
                name_entry = name_table[obj_entry["obj_id"]]
                if name_entry["flag"] == 1:
                    ref = name_entry.get("global_ref", name_entry["name"])
                    if ref in name_to_new_mesh_id:
                        new_id = name_to_new_mesh_id[ref]
                        if obj_entry.get("mesh_id") != new_id:
                            obj_entry["mesh_id"] = new_id
                            changed = True
        if changed:
            with open(local_json, "w") as f:
                json.dump(data, f, indent=2)
            patched.append(local_json.stem)
    return patched
```

Runs as part of the C++ export process. User sees: `"Patched mesh_id in: THR0000, THR0001, THR0002"`.

> **Roundtrip guarantee:** Since `.json` -> `_IPD.bin` reads `mesh_id` directly
> from JSON, updating JSON + re-running `local_json_to_ipd.py` produces a binary-correct
> IPD. Only the `mesh_id` field changes in affected `IPD_OBJ_DATA` records.

---

## 9. Constraints, Validation, and Pitfalls

### 9.1 Topology: Quad vs. Triangle

- PS1 PLM supports quads (`faces_3 != 0xFF`) and triangles (`faces_3 == 0xFF`).
- The exporter handles both automatically (via `sh1_tri_role` face attribute).
- **Validator** checks for N-gons and warns: `"THR1702 contains N-gons. These will
  be triangulated and may increase pack count."`
- Validator also warns if new pack count exceeds previous by > 50%.

### 9.2 The 7-bit tex_num Limit

- `tex_num` in `PLM_PACK_HEADER` uses 7 bits — maximum 128 unique textures per object.
- "Add Texture Slot" UI button is **disabled** at 128 entries with a descriptive tooltip.
- Validator cross-checks all `sh1_tex_num` face attributes against `len(tex_names)`.

### 9.3 Vertex Count per Submesh (255-vertex Limit)

- `faces_0..faces_3` in `PLM_PACK_HEADER` are `uint8`. `0xFF` is the triangle sentinel.
- One `PLM_DATA_HEADER` submesh can reference at most **254 unique vertices**.
- The exporter handles this automatically by splitting into multiple `PLM_DATA_HEADER`
  submeshes (controlled by `mesh_num` in `PLM_OBJ_HEADER`).
- Validator shows how many submesh splits will occur and warns if the split would
  produce an unexpectedly large number.

### 9.4 Normal Array Bounds (`num_c` vs `num_d`)

- `num_d` is confirmed as the true normal array size.
- `num_c` role is unclear. **Safe default for new geometry: `num_c = num_d`.**
- Validator checks all `normals_0..normals_3` pack indices are < `num_d`.

### 9.5 Padding Alignment

- All sections must be padded to 4-byte boundaries with `\x00` bytes.
- The new `glb_json_to_plm()` serializer must implement identical padding logic
  to `local_json_to_ipd.py`. This is required for binary roundtrip correctness.

### 9.6 The `unk_data_offset` Field

- **Hypothesis:** Points to a valid data region in the PLM; content not read by
  `sh_ipd2obj`. For edited existing files: preserve original value. For new PLM
  objects: set to `0` as a safe default.
- Validator flags `unk_data_offset != 0` on newly created objects as `[HYPOTHESIS]`.
- **Open question:** If this field is required by the game engine, new objects
  will fail in-game. Investigate before Phase 5.

---

## 10. File Structure Changes

New files required (Python prototypes kept alongside new C++ implementations):

```
tooling/
  scripts/prototypes/
    plm_manager_to_json.py    <- Prototype: Mesh -> _GLB.json serializer
    glb_json_to_plm.py        <- Prototype: _GLB.json -> binary _GLB.PLM serializer
    chunk_patcher.py          <- Prototype: patch_chunk_mesh_ids() utility
    dependency_index.py       <- Prototype: build_dependency_index() utility
  unified_cpp_editor/src/
    core/PlmManager.h/.cpp    <- NEW: Internal state and operations for PLM objects
    ui/PlmManagerWindow.h/cpp <- NEW: ImGui window implementation (or integrated into Viewport)
```

---

## 11. Implementation Phases

### Phase 1 — Read-Only Infrastructure (Window A)

**Goal:** Enforce that global objects in a chunk session are not mesh-editable.

1. In `Viewport3D` or `IpdInspector`: flag global objects.
2. In geometry modification tools: check flag and block vertex/face edits.
3. Add a visual indicator for global objects in the outliner or viewport.

**Verify:** Load `THR0000.IPD`, attempt to modify vertices of a global object -> blocked.

### Phase 2 — Dependency Index (Window B, read-only)

**Goal:** Scannable PLM data with dependency info in the ImGui panel.

1. Implement dependency indexing in C++ (using the Python prototype as reference).
2. Add `PlmManager` state to load `_GLB.PLM` (or JSON), build index, populate registry.
3. Add ImGui window with UIList and Dependent Chunks sub-panel.

**Verify:** Load `THR_GLB.PLM`. Select `THR1702`. Confirm chunk instance counts match known references.

### Phase 3 — Geometry Viewing (Window B)

**Goal:** Render all PLM objects in the ImGui PLM Manager window context.

1. In `PlmManager`: load geometry into renderer-friendly buffers.
2. Add a preview viewport specifically for the PLM Manager (or isolate it in the main `Viewport3D`).

**Verify:** All PLM objects visible with correct UVs and textures.

### Phase 4 — Export & Roundtrip (Window B)

**Goal:** Write `_GLB.PLM` back from the C++ editor; verify binary roundtrip.

1. Implement `_GLB.PLM` serialization in C++ (using the Python prototypes).
2. Add "Export" button in ImGui.
3. Verify byte-for-byte roundtrip on unmodified geometry.

**Verify:** Load `THR_GLB.PLM` -> export -> byte-compare to original `THR_GLB.PLM`.

### Phase 5 — Add / Remove / Chunk Patcher (Window B)

**Goal:** Full CRUD with automatic `mesh_id` repair.

1. Implement Add (name dialog) and Remove (dependency warning dialog) in ImGui.
2. Implement Chunk Patcher logic in C++.
3. Invoke Chunk Patcher during Export.

**Verify:** Add object, export, confirm in binary. Remove object, confirm all chunk JSONs have updated `mesh_id` values.

### Phase 6 — Validation & Budget (Window B)

**Goal:** User-visible warnings for all PS1 constraints.

1. Implement dry-run serialization to collect all warnings.
2. Implement C++ size calculation for live ImGui budget bar.
3. Add coloured indicators to ImGui list items (green = clean, orange = dirty, red = error).

---

## 12. Open Questions for the User

1. **Integration of Viewport in PLM Manager?** — Should the PLM Manager have its own dedicated 3D preview window docked inside its ImGui panel, or should it commandeer the main `Viewport3D` when an object is selected?

2. **`unk_data_offset` in new PLMs** — For brand-new PLM objects, `unk_data_offset = 0`
   is the safe default. If the game engine requires a non-zero value, new objects will
   crash in-game. Worth investigating before Phase 5.

3. **The `num_c` mystery** — When exporting edited geometry, set `num_c = num_d`
   (safe default), or try to preserve the original ratio? Preserving requires tracking
   the `num_c / num_d` ratio as per-object metadata.

4. **Per-object vs. shared tex_names** — The binary format supports per-object
   `tex_names` lists. Should the UI expose this directly, or present a shared
   "map-wide texture registry" that all objects draw from?

---

## 13. Summary of Approaches to Original Pitfalls

| Original Pitfall | Resolution |
|---|---|
| Global geometry editable from chunk session | `Viewport3D` guard blocks vertex edits. Separate ImGui context for geometry edits. |
| Live file size calculation | Fast vertex-count estimate in ImGui panel; accurate dry-run on "Verify" only. |
| Quad vs. Triangle topology | Exporter handles automatically. Validator warns on N-gons only. |
| Shared geometry blast radius | Dependency Index built at load time; shown in panel before any destructive action. |
| Texture indexing vs. Materials | 7-bit limit capped in UI. Validator cross-checks face attributes vs. tex_names list. |
| `mesh_id` reordering on delete | Stable name-based reference in JSON. Chunk Patcher auto-updates all `mesh_id`s on export. |
| No hard PLM size limit | Tiered budget bar (< 80 KB green, > 120 KB red) based on observed file sizes. |
