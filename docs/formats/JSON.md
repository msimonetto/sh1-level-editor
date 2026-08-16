# JSON Intermediate Formats & Offline Pipeline

> [!NOTE]
> **Status:** The C++ Level Editor operates directly on native binary PlayStation files (`.IPD`, `.PLM`, `.TIM`) in memory. Intermediate JSON files are no longer required for active editor workflows, but the standalone Python pipeline (`scripts/convert.py`) continues to provide full, lossless JSON round-tripping for manual inspection, debugging, and external 3D DCC tools (e.g. Blender).

---

## 1. Intermediate JSON Specifications

### A. Local Chunk Assets (`[CHUNK].json`)
The serialized representation of an individual `.IPD` file (e.g., `THR0000.json`):
- **Header & Offsets:** Preserves chunk identification (`x_pos`, `y_pos`) and section offset metadata.
- **Object Hierarchy:** Stores `IPD_POS_HEADER` groups and `IPD_OBJ_DATA` instances (Euler rotations, fixed-point translations, `glb_flag`, `mesh_id`).
- **Embedded Geometry:** Serializes local PLM polygon records, vertex tables, UVs, raw `cba` (CLUT VRAM coordinates), and `unk2` (STP transparency flag).
- **Embedded Collision:** Serializes subcells, split vertices, surface planes, and broadphase grids when present.

### B. Map Global Assets (`[PREFIX]_GLB.json`)
The dictionary of shared level geometry and textures extracted from `_GLB.PLM` and related `.TIM` files for a given map prefix (e.g., `THR_GLB.json`):
- Groups global prop meshes referenced by local chunks when `glb_flag == 1`.
- Scopes texture names and mesh IDs per map stage without monolithic asset duplication.

### C. Room Linkage Overlays (`[MAP].json`)
Located in `data/workspace/overlays/`, these represent decompiled map event headers (`header_field_D2C.h` / `map_points.h`):
- Stores arrival waypoints, door links, trigger volumes (AABB, OBB, radius), and destination map state indices.

---

## 2. Dual-PNG Texture Architecture

To preserve PS1 15-bit RGB + 1-bit STP (Semi-Transparency) data without lossy 32-bit baking, `.TIM` files convert into paired PNG files:

1. **Indexed Image (`[name].png`):** 8-bit indexed PNG holding raw pixel indices (0–15 or 0–255), previewed with CLUT #0.
2. **Palette Strip (`[name]_cluts.png`):** 32-bit RGBA PNG where each horizontal row represents one 16-color CLUT variant:
   - `0x0000` (STP 0) $\rightarrow$ `RGBA(0, 0, 0, 0)` (Masked Transparent)
   - `0x8000` (STP 1) $\rightarrow$ `RGBA(0, 0, 0, 128)` (Semi-transparent Black)
   - RGB (STP 0) $\rightarrow$ `RGBA(R, G, B, 255)` (Opaque)
   - RGB (STP 1) $\rightarrow$ `RGBA(R, G, B, 128)` (Semi-transparent)

---

## 3. Standalone Python CLI (`scripts/convert.py`)

Manual bidirectional conversions are executed via the modular CLI:

```bash
# IPD <-> JSON
python scripts/convert.py ipd-to-json <file.IPD> [-a <assets_dir>]
python scripts/convert.py json-to-ipd <file.json> [-o <out.IPD>]

# PLM <-> JSON
python scripts/convert.py plm-to-json <file.PLM>
python scripts/convert.py json-to-plm <file.json> [-o <out.PLM>]

# TIM <-> PNG
python scripts/convert.py tim-to-png <file.TIM> <out_dir>
python scripts/convert.py png-to-tim <indexed.png> <cluts.png> <out.TIM>
```
