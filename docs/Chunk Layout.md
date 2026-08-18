# World Assembly & Collision Grid Structure

This document outlines how Silent Hill 1 handles the assembly of IPD chunks (local static models/collision data) into contiguous areas, and how logical "rooms" are represented. This information is derived from the trusted PC port technical analysis and the upstream decompilation documentation.

## 1. The Collision Grid (IPD Chunks)

The game world is conceptually divided into a 2D spatial grid, but the entire grid is never loaded at once due to the PlayStation 1's memory constraints. 

- **IPD Chunks**: The world geometry is split into individual chunks stored in `.IPD` files on the disc. These chunks bake static collision data (walls, floors) and geometry layout.
- **Dynamic Streaming**: The game dynamically streams these IPD chunks into memory. The global state (`g_Map` / `s_MapTerrain`) tracks a maximum of **4 IPD chunks** at any one time (`activeChunks[4]`).
- **Cell Mapping**: At map load, `Map_MakeIpdGrid` scans the global file table for all `FileType_Ipd` files whose filenames match the current map's `mapTag` (e.g. `"THR"`), parses the hex X/Z coordinates from each filename, and slots each file index into a spatial lookup grid (`s_MapTerrain.chunkGrid`). As the player character moves, `Map_PlaceIpdAtCell` queues file reads to replace chunks that are no longer needed.
- **Collision Queries**: When an entity moves or collision is tested (via functions like `Collision_SurfaceGet`), the game translates its world `(X, Z)` coordinates into `(cellX, cellZ)`. The collision lookup walks the 4 active chunk slots to find the chunk holding that cell and fetches its `s_IpdCollisionData`. 

This mechanism allows separate IPD models to seamlessly form a contiguous world space. If an entity steps outside the loaded grid, the collision system returns a sentinel "void" result (`groundHeight = Q12(8.0f)` — infinitely low), causing characters to fall rather than get stuck, until the correct chunk streams in.

## 2. Representation of "Rooms"

In Silent Hill 1's engine, **there is no first-class "room" object**. The engine only natively understands world coordinates, IPD chunks, and map overlays. 

"Rooms" are instead a logical construct formed by:
1. **Map Overlays**: Sets of data and logic explicitly grouped together (e.g., `map0_s00`).
2. **Door Transitions**: Doors function as boundaries that partition the contiguous collision grid. Navigating a door often unloads the current map overlay/chunk group and loads a new one. In contexts like randomizers or modding, "room-by-room" logic is achieved by harvesting the set of doors linking maps and IPDs.
3. **Audio Context (BGM Limits)**: The background music system explicitly tracks transitions by applying "per-room" layer limits (`s_BgmLayerLimits`). The music engine adjusts audio mixing when the game signals a transition into a new logical room.
4. **Spawn Initialization**: Enemy spawns are often evaluated on a per-room basis (e.g., via `Game_NpcRoomInitSpawn`), proximity-triggered upon entering the room's bounds.

## 3. Dynamic Walls and Trigger Zones

- **Dynamic Doors / Cutscene Blockers**: While the IPD collision grid is mostly static, specific wall elements can be dynamically enabled or disabled per-query. A 16-bit wall mask (`s_CollisionState::field_2`) allows the game to turn specific walls on or off. This is how the engine handles lockable doors or temporary invisible walls during cutscenes.
- **Trigger Zones**: Despite the name, "Trigger Zones" are not event or transition triggers. They are strictly an elevation system—axis-aligned bounding boxes placed to define raised floors, steps, or platforms.

## Conclusion

The feeling of "rooms" and "contiguous areas" in Silent Hill 1 is achieved not by a single monolithic data structure, but by seamlessly streaming a 2D grid of 4-resident IPD chunks, and using event-driven logic (doors, spawners, and BGM limits) to carve that continuous grid into distinct logical spaces.
