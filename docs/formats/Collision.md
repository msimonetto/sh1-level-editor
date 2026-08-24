# IPD Collision Format Reference

Map-level collision and physics data is optionally embedded within chunk `.IPD` files immediately following the main `IPD_FILE_HEADER` (at offset `0x54`), detected when `obj_name_offset - sizeof(IPD_FILE_HEADER) == 308`.

C++ definitions are located in [`include/core/Structs.h`](../../include/core/Structs.h) and processed in [`include/viewport/CollisionOverlay.h`](../../include/viewport/CollisionOverlay.h) / [`src/viewport/CollisionOverlay.cpp`](../../src/viewport/CollisionOverlay.cpp).

---

## 1. Binary Structures

### IPD_COLL_HEADER (308 bytes / 0x134)
The root collision header starting at offset `0x54`. All `ptr_*` fields store byte offsets relative to the start of this header (`0x54`).

```c
struct IPD_COLL_HEADER {
    int32_t  positionX;             // Chunk collision world offset X (q23.8)
    int32_t  positionZ;             // Chunk collision world offset Z (q23.8)
    uint8_t  splitVertexCount;      // Count of IPD_COLL_SVECTOR entries
    uint8_t  surfaceCount;          // Count of IPD_COLL_SURFACE entries
    uint8_t  subcellCount;          // Count of IPD_COLL_SUBCELL entries
    uint8_t  unkBlock3Count;        // Count of IPD_COLL_UNK3 entries
    int32_t  ptr_splitVertices;     // Relative offset -> IPD_COLL_SVECTOR[] (count * 6B)
    int32_t  ptr_surfaces;          // Relative offset -> IPD_COLL_SURFACE[] (count * 12B)
    int32_t  ptr_subcells;          // Relative offset -> IPD_COLL_SUBCELL[] (count * 10B)
    int32_t  ptr_unkBlock3;         // Relative offset -> IPD_COLL_UNK3[] (count * 10B)
    uint16_t gridScale;             // Broadphase cell dimension (typically 512 / 0x0200)
    uint8_t  gridWidth;             // Grid cell count along X (typically 20)
    uint8_t  gridHeight;            // Grid cell count along Z (typically 20)
    int32_t  ptr_grid;              // Relative offset -> Broadphase range array (gridWidth * gridHeight * 4B)
    uint16_t block5Count;           // Count of block 5 indirection bytes
    uint16_t block6Count;           // Count of block 6 indirection bytes
    int32_t  ptr_block5;            // Relative offset -> uint8_t[] subcell index table
    int32_t  ptr_block6;            // Relative offset -> uint8_t[] subcell index table
    int32_t  ptr_unk7;              // Reserved / unused pointer
    uint8_t  subcellCheckIdx[256];  // Per-frame deduplication buffer (runtime state)
};
```

### IPD_COLL_SVECTOR (6 bytes)
2D/3D split line vertices used to construct subcell boundaries and vertical wall planes.
```c
struct IPD_COLL_SVECTOR {
    int16_t x;
    int16_t y;
    int16_t z;
};
```

### IPD_COLL_SURFACE (12 bytes)
Defines ground height, tilt angles, and surface material attributes.
```c
struct IPD_COLL_SURFACE {
    int16_t  field_0;           // Relative X (q7.8)
    int16_t  baseGroundHeight;  // Base floor elevation Y (q7.8)
    int16_t  field_4;           // Relative Z (q7.8)
    uint16_t tilt_flags;        // Surface properties and wall triggers:
                                //   bits [0..4]: Ground material / sound ID (12 = physical wall)
                                //   bits [5..7]: Camera barrier flags
    int16_t  tiltAngleX;        // Pitch gradient (q7.8)
    int16_t  tiltAngleZ;        // Roll gradient (q7.8)
};
```

### IPD_COLL_SUBCELL (10 bytes)
Spatial subcell linking bisecting split vertices to left/right surface definitions.
```c
struct IPD_COLL_SUBCELL {
    int16_t field_0;            // Packed X coordinate (14-bit sign-extended + 2-bit flag)
    int16_t field_2;            // Packed Y coordinate (14-bit sign-extended + 2-bit flag)
    int16_t field_4;            // Z coordinate (q7.8)
    uint8_t splitVertexIdx0;    // Index into splitVertices array
    uint8_t splitVertexIdx1;    // Index into splitVertices array
    uint8_t surfaceIdx0;        // Index into surfaces array (255 = void/null)
    uint8_t surfaceIdx1;        // Index into surfaces array (255 = void/null)
};
```

### IPD_COLL_UNK3 (10 bytes)
Optional collision/trigger sub-block records.
```c
struct IPD_COLL_UNK3 {
    uint16_t flags;
    int16_t  offsetX;
    int16_t  offsetY;
    int16_t  offsetZ;
    int16_t  field_8;
};
```

---

## 2. Editor Reconstructed Characteristics

As implemented in [`CollisionOverlay`](../../include/viewport/CollisionOverlay.h) ([`src/viewport/CollisionOverlay.cpp`](../../src/viewport/CollisionOverlay.cpp)), collision data resolves into three primary visual representations:

### 1. Broadphase Terrain Meshes (`terrainBatches`)
- **Grid Resolution:** Lookups across the `gridWidth` × `gridHeight` grid sample `ptr_block5` and `ptr_block6` for candidate `subcells`.
- **Elevation:** Ground Y is assigned from `baseGroundHeight` of the active `IPD_COLL_SURFACE`.
- **Walkability:** Cells are categorized as walkable unless `tilt_flags & 0x1F == 12` or camera barrier flags `((tilt_flags >> 5) & 7) != 0` are set.

### 2. Extruded Wall Barriers (`wallBatches`)
Vertical wall geometry is dynamically extruded between split vertex pairs (`splitVertexIdx0` → `splitVertexIdx1`) based on surface flags:
- **`WALL_PHYSICAL`** (`tilt_flags & 0x1F == 12`): Solid movement blocker (extruded height ~2.5 units).
- **`WALL_CAMERA`** (`((tilt_flags >> 5) & 7) != 0`): Camera obstruction barrier (extruded height ~3.5 units).
- **`WALL_VOID`** (`surfaceIdx == 255`): Out-of-bounds or open void perimeter (extruded height ~1.5 units).

### 3. Subcell Floor Boundaries (`floorLines`)
- 3D line segments drawn along subcell split edges (`splitVertexIdx0` to `splitVertexIdx1`).
- Color-coded deterministically by ground material / footstep sound ID (`tilt_flags & 0x1F`).

### 4. Coordinate Transformations
Raw fixed-point coordinates convert to OpenGL world space via:
$$\text{World} = \left( \frac{\text{rawX} + \text{positionX}}{256.0}, \; -\frac{\text{rawY}}{256.0}, \; -\frac{\text{rawZ} + \text{positionZ}}{256.0} \right)$$
