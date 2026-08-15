# Map-Specific Asset Pipeline & Formats

This document describes the intermediate JSON and image structures used to bridge the conversion between binary PlayStation files (`.IPD`, `.PLM`, `.TIM`) and workable 3D data formats (`.OBJ`, `.MTL`, `.PNG`).

The workflow operates on a **2-Tier Map-Specific Decoupling System** designed to minimize file size, remove duplicate geometry, mirror the game's actual VRAM loading lifecycle, and provide lossless visual editing for textures.

## 1. Map-Specific Global Assets (`[PREFIX]_GLB.json`)
**Purpose**: The central dictionary of all geometry and texture assets parsed from the binary `_GLB.PLM` and `.TIM` files for a specific map prefix (e.g., `THR_GLB.json`).
- Replaces the deprecated monolithic `master_assets.json`.
- Accurately mirrors PS1 DMA loading: assets are scoped only to their relevant map area, eliminating artificial naming collisions (e.g., prepending `THR_` to object names).
- Serves as the ultimate source of truth for global meshes loaded during a specific map's session.

## 2. Local Chunk Assets (`[CHUNK].json`)
**Purpose**: The direct, lightweight serialized representation of a single `.IPD` file (e.g., `THR0000.json`).
- Stripped of all global geometry arrays.
- Contains localized data unique to the chunk:
  - Header offsets.
  - Placement coordinates (Euler rotations, fixed-point translations).
  - Object Flags (`glb_flag`).
  - Polygon data including exact `cba` VRAM coordinates (for CLUT selection) and `unk2` transparency flags.
- Enables perfectly lossless, byte-for-byte backwards conversion back to the binary `.IPD` structure.

## 3. Lossless Texture & CLUT Storage
To preserve the 15-bit RGB + 1-bit STP (Semi-Transparency) data while remaining visually editable in modern tools like Aseprite, `.TIM` files are split into two companion `.png` files rather than baked into a single lossy 32-bit `.TGA`.

### A. The Image Data (`[texture_name].png`)
- **Format:** 8-bit Indexed PNG.
- **Content:** Contains raw pixel indices (0-15 or 0-255).
- **Visuals:** Embeds **CLUT #0** as its internal palette for correct visual rendering. Index `0x00` (transparent mask) has its Alpha channel set to 0.
- **Editing:** Editable in Aseprite; saving updates the raw pixel indices without modifying color data.

### B. The Palette Data (`[texture_name]_cluts.png`)
- **Format:** 32-bit TrueColor RGBA PNG.
- **Content:** Each row of pixels represents one CLUT variant from the original `.TIM`.
- **Transparency Mapping:** The PS1 STP bit is mapped to the Alpha channel:
  - `0x0000` (Black, STP 0) $\rightarrow$ `RGBA(0, 0, 0, 0)` (Mask)
  - `0x8000` (Black, STP 1) $\rightarrow$ `RGBA(0, 0, 0, 128)` (Semi-transparent Black)
  - RGB, STP 0 $\rightarrow$ `RGBA(R, G, B, 255)` (Opaque)
  - RGB, STP 1 $\rightarrow$ `RGBA(R, G, B, 128)` (Semi-transparent)
- **Editing:** Can be opened in image editors to physically paint or tweak palette colors. The script converts these RGBA values perfectly back to 16-bit PS1 colors.

## 4. Polygon Rendering (OBJ/MTL)
- Polygons no longer bake CLUTs into a massive TrueColor texture.
- `.OBJ` files map directly to the base `[texture_name].png`.
- The `cba` value (CLUT offset) and `unk2` (STP flag) are preserved in the `.json` and passed into the `.OBJ` (via custom UV maps or vertex colors), allowing custom shaders to replicate the PS1 rendering dynamically.
