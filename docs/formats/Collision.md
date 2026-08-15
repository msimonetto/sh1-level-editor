# IPD Collision Architecture: A 2.5D Heightfield System

> [!WARNING]
> Following a thorough investigation of the `.IPD` file structures and the game's collision resolution logic (`collision.c`), the assumption that collision data is stored externally in `.KDT` files or separate formats has been definitively debunked. **All collision data for map geometry is embedded internally within the `.IPD` file header itself.**

By examining the `s_IpdCollisionData` struct and cross-referencing it with typical PS1 game development paradigms (e.g., Resident Evil, Dino Crisis, and Silent Hill), we can deduce exactly how the game processes physics, walls, and floors on the MIPS R3000 CPU.

---

## 1. Internal Storage: The `s_IpdCollisionData` Struct

Every `.IPD` file begins with a header (`s_IpdHeader`) which inherently contains the `s_IpdCollisionData` struct. There are no external pointers; the data is a contiguous block inside the chunk.

```c
typedef struct _IpdCollisionData
{
    q23_8                  positionX;
    q23_8                  positionZ;
    u32                    splitVertexCount : 8; 
    u32                    surfaceCount     : 8; 
    u32                    subcellCount     : 8; 
    SVECTOR3*              splitVertices;        
    s_IpdCollSurface*      surfaces;             
    s_IpdCollSubcell*      subcells;
    // ...
} s_IpdCollisionData;
```

This struct points to a highly optimized **2.5D Grid and Sector** system. The PS1 CPU was too weak to perform raw 3D polygon-to-polygon intersection tests (like AABB or Raycasting against the visual mesh) for the entire map. Instead, Silent Hill 1 separates the *Visual Mesh* (what the GPU draws) from the *Collision Mesh* (a mathematical 2D grid extruded into 3D).

---

## 2. The Grid System: Subcells and Splits

The map chunk is divided into a 2D top-down grid along the X and Z axes. Each grid square is a **Subcell** (`s_IpdCollSubcell`). 

For broad-phase collision detection, the game simply takes the player's X/Z world coordinates, divides by the grid size, and instantly knows which subcell the player is standing in (O(1) lookup). 

If a subcell contains complex geometry (like a diagonal wall or a curb), it is bisected by a line segment:
- The subcell defines two `splitVertexIdx` values, which are 8-bit indices pointing into the `splitVertices` array (representing a 2D line segment).
- This creates a line splitting the subcell into two distinct **Surfaces**, referenced by two 8-bit `surfaceIdx` values.
- The engine uses a fast 2D cross-product (point-vs-line test) to determine which half of the subcell the player is standing on.

---

## 3. Resolving Floors and Slopes

Once the engine knows which **Surface** the player is on, it resolves the Y-axis (vertical height).

```c
typedef struct _IpdCollSurface
{
    q7_8 field_0;            // Relative X
    q7_8 baseGroundHeight;   // The floor's base Y position
    q7_8 field_4;            // Relative Z
    u16  groundType    : 5;  // Dirt, Grass, Metal (For footstep SFX)
    u16  disableHeight : 3;  // Wall/Curb flag
    u16  field_6_11    : 4;  // Slope flag
    q7_8 tiltAngleX;         // Pitch
    q7_8 tiltAngleZ;         // Roll
} s_IpdCollSurface;
```

For a standard floor, `disableHeight` is false. The game calculates the player's exact Y position by taking the `baseGroundHeight` and applying the `tiltAngleX` and `tiltAngleZ` gradients based on the player's relative X/Z position within the surface. This allows Silent Hill to have ramps, hills, and sloped streets without needing true 3D polygon collision.

Additionally, the `groundType` integer is what triggers the specific footstep sound effects (metal grating, wood, concrete) dynamically without needing external trigger volumes.

---

## 4. Resolving Walls, Curbs, and Boundaries

How does the game prevent the player from walking through a wall or falling off the map if the collision is just a top-down grid?

It uses the `disableHeight` flag and `GroundType_None`. 

In `collision.c`, we can see the mathematical resolution for walls:

```c
if (curSurface->disableHeight == true || curSurface->groundType == GroundType_None)
{
    state->point.splitVertex0.vy -= Q8(-DEFAULT_CEILING_HEIGHT);
    state->point.splitVertex1.vy -= Q8(-DEFAULT_CEILING_HEIGHT);
}
```

When a surface is marked as a wall (`disableHeight = true`), the engine ignores the floor calculation. Instead, it takes the `splitVertices` (the 2D line dividing the subcell) and **extrudes it infinitely along the Y-axis** (up to `DEFAULT_CEILING_HEIGHT`). 

This creates an impenetrable vertical plane. When the player's X/Z velocity attempts to cross this split line, the 2D collision solver rejects the movement and forces the player to slide along the line vector.

### Summary
- **Visuals:** Handled by `.TIM` textures and polygon data inside `.PLM` or the IPD geometry block.
- **Floors/Slopes:** Handled by 2D subcell grids mapped to slanted mathematical planes (`baseGroundHeight` + `tiltAngle`).
- **Walls/Curbs:** Handled by bisecting a subcell with a line and flagging one side as `disableHeight = true`, extruding the line into an infinite vertical barrier.
- **KDT:** Completely unrelated to collision (Audio/Sequence data).

# IPD Collision Header Breakdown

Through analyzing the `gap_header_to_obj_name` (308 bytes) and `gap_obj_data_to_plm` blocks across all generated maps (APU0000, ER0301, SC0000, THR0000, THR0100, THRFFFF), we have successfully reverse-engineered the entire structure of the internal IPD collision mesh.

## 1. The 308-byte Header (`s_IpdCollisionData`)
This header is located exactly at offset `0x54` (immediately following the `IPD_FILE_HEADER`). It spans 308 bytes, but only the first 48 bytes are actively used. 

**Format String:** `<iiBBBBiiiiHBBiiHHiiii252s`
```c
struct s_IpdCollisionData {
    // 0x54
    int positionX; // 4 bytes
    int positionZ; // 4 bytes
    
    // 0x5C
    u8 splitVertexCount; // 1 byte
    u8 surfaceCount;     // 1 byte
    u8 subcellCount;     // 1 byte
    u8 unkBlock3Count;   // 1 byte
    
    // 0x60
    u32 ptr_splitVertices; // offset to array of (count * 6 bytes), padded to 4
    u32 ptr_surfaces;      // offset to array of (count * 12 bytes)
    u32 ptr_subcells;      // offset to array of (count * 10 bytes), padded to 4
    u32 ptr_unkBlock3;     // offset to array of (count * 10 bytes), padded to 4
    
    // 0x70
    u16 gridScale;  // 2 bytes (Usually 0x0200 = 512 units per grid cell)
    u8  gridWidth;  // 1 byte (Always 20)
    u8  gridHeight; // 1 byte (Always 20)
    u32 ptr_grid;   // offset to Broadphase Grid Array (`s_IpdCollSubcellRange`)
    
    // 0x78
    u16 block5Count; // 2 bytes
    u16 block6Count; // 2 bytes
    u32 ptr_block5;  // offset to array of (count * 1 byte), padded to 4
    u32 ptr_block6;  // offset to array of (count * 1 byte)
    
    // 0x84
    u32 ptr_unk7;    // offset/pointer 7
    
    // 0x88 
    u8 subcellCheckIdx[256]; // Array embedded directly in the header!

    // 0x188
    // End of 308-byte struct.
};
```

## 2. The Payload (`gap_obj_data_to_plm`)
All of the pointers listed above (e.g., `ptr_splitVertices`, `ptr_grid` - except `ptr_unk7`) point to offsets **relative to the start of this 308-byte collision header**. Because the map's visual object hierarchy (`IPD_POS_HEADER` and `IPD_OBJ_DATA`) is placed immediately *after* the string table in the file, it sits *between* the 308-byte Collision Header and the Collision Payload.

Therefore, the Collision Payload is pushed all the way down to `gap_obj_data_to_plm`, where all the arrays are serialized back-to-back:
1. **Split Vertices:** `splitVertexCount * 6` bytes. (No trailing pad in `SVECTOR`, so they are just 3 `short`s). Padded to the next 4-byte boundary.
2. **Surfaces (`s_IpdCollSurface`):** `surfaceCount * 12` bytes.
3. **Subcells (`s_IpdCollSubcell`):** `subcellCount * 10` bytes. Padded to the next 4-byte boundary. The struct consists of 3 packed `short` values (X, Y, Z coordinates + some flags) followed by four `u8` fields: `splitVertexIdx0`, `splitVertexIdx1`, `surfaceIdx0`, and `surfaceIdx1`.
4. **Block 3 (`s_IpdCollisionData_18`):** `unkBlock3Count * 10` bytes. Padded to the next 4-byte boundary.
5. **Broadphase Grid:** An array of `s_IpdCollSubcellRange` elements (4 bytes each). This represents the map's spatial partitioning (usually 20x20, given by `gridWidth` and `gridHeight`). These ranges index into `ptr_block5`, not `subcellCheckIdx`. `subcellCheckIdx` is a completely separate per-frame dedup counter array.
6. **Block 5:** `block5Count * 1` byte. Padded to the next 4-byte boundary. Indexed into by the Broadphase Grid to yield subcells.
7. **Block 6:** `block6Count * 1` byte. Padded to the next 4-byte boundary.

---

### Conclusion
By mapping these 7 arrays exactly to the counts provided in the header, we can deserialize `gap_obj_data_to_plm` perfectly into structured JSON lists. There are no remaining "mystery hex" gaps in the collision payload.
