# Manual Lossless Conversions with JSON

This guide demonstrates how to use the standalone Python conversion CLI ([`scripts/convert.py`](../scripts/convert.py)) to convert binary PlayStation assets (`.IPD`, `.PLM`, `.TIM`) to human-readable JSON / PNG intermediates and reconstruct them back into byte-accurate PS1 binaries.

> [!NOTE]
> While the graphical C++ Level Editor edits `.IPD`, `.PLM`, and `.TIM` binaries directly in RAM, `scripts/convert.py` remains the primary tool for offline batch processing, manual JSON property inspection, text-based scripting, and external DCC workflows (e.g., Blender and Aseprite).

---

## Prerequisites

Ensure you have Python 3.9+ installed along with `Pillow` for texture operations:

```bash
pip install pillow
```

All commands below should be run from the repository root directory (`sh1-level-editor/`).

---

## 1. IPD Map Chunk Conversions (`.IPD` $\leftrightarrow$ `.json`)

The `.IPD` format contains chunk layout nodes, local polygon meshes, texture assignments, and 2.5D collision data.

### Exporting `.IPD` to JSON
To extract a binary chunk into an editable JSON file:

```bash
python scripts/convert.py ipd-json data/assets/THR0000.IPD -o data/workspace/THR0000.json
```

**Optional Flags:**
- `--assets-dir <path>`: Specifies directory containing `[PREFIX]_GLB.json` (e.g., `data/workspace/`) to resolve shared global prop references during serialization.
- `--output`, `-o <path>`: Destination path (defaults to same path with `.json` extension).

### Reconstructing Binary `.IPD` from JSON
To recompile a modified JSON file back into a native binary `.IPD`:

```bash
python scripts/convert.py json-ipd data/workspace/THR0000.json -o data/workspace/THR0000.IPD
```

---

## 2. PLM 3D Model Conversions (`.PLM` $\leftrightarrow$ `.json`)

`.PLM` files store 3D mesh collections, vertex normal tables, quad-face definitions, and texture string tables (used for both standalone models and global stage props like `THR_GLB.PLM`).

### Exporting `.PLM` to JSON
```bash
python scripts/convert.py plm-json data/assets/THR_GLB.PLM -o data/workspace/THR_GLB.json
```

### Reconstructing Binary `.PLM` from JSON
```bash
python scripts/convert.py json-plm data/workspace/THR_GLB.json -o data/workspace/THR_GLB.PLM
```

---

## 3. TIM Texture Conversions (`.TIM` $\leftrightarrow$ Dual PNG)

PlayStation `.TIM` textures store 15-bit BGR colors with 1-bit Semi-Transparency (STP) and multiple Color Lookup Tables (CLUTs). Rather than flattening into a lossy 32-bit image, `convert.py` splits each texture into two companion PNG files.

### Decompiling `.TIM` to Dual-PNG
```bash
python scripts/convert.py tim-png data/assets/THR_MROA1.TIM data/workspace/textures/THR_MROA1
```

This generates:
1. `THR_MROA1.png`: An 8-bit indexed PNG storing raw pixel indices (0–15 or 0–255) previewed against CLUT row #0.
2. `THR_MROA1_cluts.png`: A 32-bit RGBA palette strip where each row represents one 16-color CLUT variant.

### Recompiling Dual-PNG to Binary `.TIM`
After painting in image editors like Aseprite or Photoshop:

```bash
python scripts/convert.py png-tim data/workspace/textures/THR_MROA1 data/workspace/textures/THR_MROA1.TIM
```

*(Note: pass the path stem without `.png` extension as the first argument).*

---

## 4. Wavefront OBJ Export (`.json` $\rightarrow$ `.OBJ`)

To inspect or import an extracted IPD chunk into external 3D software (Blender, Maya, 3ds Max):

```bash
python scripts/convert.py json-obj data/workspace/THR0000.json --assets-dir data/workspace/textures/ --out-dir data/workspace/obj/
```

**Parameters:**
- `json_file`: The source chunk JSON intermediate.
- `--assets-dir`: Directory containing companion texture images.
- `--out-dir`: Output folder where `.OBJ`, `.MTL`, and mapped textures are emitted.
- `--no-unused`: (Optional) Skip emitting unreferenced mesh clusters.

---

## 5. End-to-End Demonstration Walkthrough

Below is a complete example modifying an object's translation within a chunk:

1. **Extract binary IPD to JSON:**
   ```bash
   python scripts/convert.py ipd-json data/assets/THR0000.IPD -o data/workspace/THR0000.json
   ```

2. **Edit properties in `THR0000.json`:**
   Open `data/workspace/THR0000.json` in any text editor. Locate the desired object node under `obj_data`:
   ```json
   {
       "tx": 10240,
       "ty": 0,
       "tz": -5120,
       "rx": 0,
       "ry": 1024,
       "rz": 0,
       "glb_flag": 0,
       "mesh_id": 2
   }
   ```
   Modify coordinates (e.g., change `tx` or `ty`).

3. **Rebuild binary `.IPD`:**
   ```bash
   python scripts/convert.py json-ipd data/workspace/THR0000.json -o data/workspace/THR0000.IPD
   ```

4. **Verify in C++ Editor / Game:**
   Load the chunk in the Level Editor or copy to your game build folder to observe the updated placement in-engine.
