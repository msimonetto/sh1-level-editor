# IPD Naming Conventions

This document describes the naming convention for `.IPD` (map/world geometry) files. The convention is **engine-enforced**: `Map_MakeIpdGrid` in the PC port decomp scans filenames with this exact pattern to build the spatial chunk grid at runtime. Filenames follow a strict structural pattern that denotes both the **local map location** (prefix) and the **relative chunk coordinates** (last four characters).

## File Name Structure

Every IPD file name matches the following regex pattern:
`^([A-Z]+)([0-9A-F]{2})([0-9A-F]{2})\.IPD$`

For example, `THR01FD.IPD` can be broken down into:

- **`THR`**: Map Prefix (Town)
- **`01`**: X Coordinate
- **`FD`**: Z Coordinate

### Chunk Coordinates (X and Z)

The map chunks are organized in a 2D grid relative to a logical center `(0, 0)`. The coordinates are represented by two 8-bit signed hexadecimal integers:

- Values from `00` to `7F` represent positive coordinates `(0 to 127)`.
- Values from `80` to `FF` represent negative coordinates `(-128 to -1)` using two's complement. For example, `FF` is `-1`, `FE` is `-2`, `FD` is `-3`.

**Note:** While the filenames encode their placement relative to `(0, 0)` in map chunks, when these files are parsed into OBJ format (and their intermediate JSON), the vertices are placed in their absolute world coordinates.

## Map Prefixes

The 2- or 3-letter prefixes correspond to specific locations in Silent Hill. The suffix **`U`** generally stands for **"Ura"** (Japanese for Reverse/Alternate), denoting the Nightmare version of a location.

| Prefix | Count | Location | Notes |
| :--- | :--- | :--- | :--- |
| **`THR`** | 128 | Old Silent Hill (Town) | Covers the expansive outdoor town map. Does not include Central Silent Hill. |
| **`SC`** | 42 | Midwich Elementary School (Normal) | Chunks are structurally disconnected from each other. |
| **`SU`** | 40 | Midwich Elementary School (Alternate) | "School Ura". Matches `SC` in chunk count, also heavily disconnected. |
| **`HP`** | 20 | Alchemilla Hospital (Normal) | Standard mapping. |
| **`HU`** | 58 | Alchemilla Hospital (Alternate) | "Hospital Ura". Alternate hospital is larger/more complex. |
| **`RSR`** | 27 | Resort Area (Normal) | Covers the Resort Area outdoors. |
| **`RSU`** | 23 | Resort Area (Alternate) | "Resort Ura". Includes textures for Lighthouse (`RSULH*`) and Boat. |
| **`DR`** | 17 | Sewers (Drainage) | Matches the path to the Resort Area. |
| **`DRU`** | 6 | Sewers (Alternate / Part 2) | Likely the second section of the sewers or a darker variant. |
| **`APU`** | 9 | Amusement Park (Alternate) | "Amusement Park Ura". The park is only visited in the Alternate world. |
| **`SPR`** | 32 | Central Silent Hill (Town Center) | Central Silent Hill streets. |
| **`SPU`** | 33 | Alternate Central Silent Hill | Alternate Central Silent Hill (including the alternate shopping center). |
| **`ER`** | 58 | Event / Extra Rooms (Interiors) & Nowhere | Isolated interiors: Balkan Church (`ER_CHRC`), School Bus (`ER_BUS`), Norman's Motel (`ER_MT*`), Annie's Bar (`ER_BILL`). Also contains Nowhere puzzle rooms that replace existing spaces in-game. |

### Disconnected Rooms and Nowhere

Some areas are structurally disconnected from the main grid or even from each other. For example:

- **`ER` Prefix:** Standalone interior locations (e.g., shops, the church). However, they are still assigned `X` and `Z` coordinates in their filenames, likely based on where their entrance exists on the overarching town map (`THR` or `RSR`), making the naming convention consistent regardless of physical connection.
- **`SC` / `SU` Prefixes:** The school rooms and corridors are often stored as entirely disconnected chunks, which is notable for how the game logic handles area transitions.
- **Nowhere:** The final area of the game ("Nowhere") does not appear to have its own unique map prefix. It is constructed by reusing rooms and assets from other prefixes (e.g. `HU`, `SU`, `ER`) and uses specific `ER` files for unique puzzle rooms (like the names/ages list) that replace existing chunks in-game.

### Known Conversion Issues (Drainage/Sewers)

When converting `DR` and `DRU` (Sewers) files, you may encounter missing texture dependencies (such as `DWAVE.TIM`, likely used for water effects) that are absent from the provided `complete` folder. This can cause the OBJ export script to fail at generating faces, resulting in a mesh that consists only of vertices.

