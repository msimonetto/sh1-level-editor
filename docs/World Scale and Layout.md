# Custom Map Architecture & Technical Nuances

This document expands on the world assembly and collision details discovered during our research. It translates technical collision facts and map structures into practical considerations for our long-term objective: **authoring custom maps**.

## 1. World Scale and Chunk Limitations

When designing geometry and modeling for custom maps, the scaling must adhere strictly to the engine's built-in math:

- **The World-Unit Scale:** `1 world-unit` is exactly equal to `1 meter`. This is derived from the height conversion formula, where a single height unit in trigger zones calculates to 0.5 world-units (labeled in source as half-meters).
- **Chunk Size Limit:** The engine strictly enforces that an IPD chunk (a cell) is **40x40 world-units (40x40 meters)**. 
  - **Custom Map Application:** When modeling custom levels, the geometry must be sliced into a grid of 40x40 meter IPD files. If a model exceeds this bounding box, the `(cellX, cellZ)` lookup function (`func_800426E4`) will fail to fetch collision properly for out-of-bounds geometry.

## 2. Border Overlap and Boundary Buffers

The engine has built-in mechanisms to ensure transitions between chunks are seamless.

- **Trigger Zone Sensing Radius:** Elevation trigger zones use an extended boundary buffer of **±16 meters**. This acts as an early-warning sensing radius to cache the zone before the player steps on it.
- **Sub-Cell Collision Overlap:** The IPD grid intentionally allows sub-cells to overlap at boundaries to prevent entities from slipping through cracks between chunks. The engine prevents redundant collision checks (testing the same wall 4 times) by using a timestamp array (`visitedStamps`) tied to a query generation counter.
  - **Custom Map Application:** We do not need to worry about perfectly flush, zero-tolerance borders between IPD chunks. Slight overlaps of collision geometry across the 40-meter boundaries are standard and expected by the engine.

## 3. Collision Cylinders vs. Visual Frustums

It is crucial to differentiate what the player *sees* from what the engine *collides* with.

- **Rendering:** The game uses traditional PlayStation 1 `libgte` (Geometry Transformation Engine) mathematics. Polygons are visually clipped and culled using a camera view frustum.
- **Character Collision:** Characters and enemies interact with each other and the world using **animated cylinders**, not polygonal meshes. The radius and height of this cylinder update dynamically based on keyframes.
  - **Custom Map Application:** Because characters are cylinders (and enemies like the Air Screamer have specific push radiuses), custom corridors and doors must be modeled wide enough to accommodate these cylinders. Overly tight geometry can result in enemies pushing the player's cylinder through walls.

## 4. Components of a Map Overlay

Looking at `map7_s02` (Nowhere) as an example, a full map overlay in Silent Hill 1 is composed of several discrete logic files. To inject a fully functional custom map, we will need to generate or define these components:

- **`header_field_D2C.h`**: The trigger zones. Defines where raised floors, platforms, and steps are.
- **`map_points.h`**: Points of Interest (POIs). Used to define exact coordinates for door transitions and event triggers.
- **`vc_road_data.h`**: Camera data. Dictates the bounds and behavior of the game's dynamic camera system for the map.
- **`chara_spawns.h`**: Spawn configurations dictating what enemies load in the room.
- **`[map_name]_events_data.c`**: The scripted logic for the area, linking door items, dialogue, and unlocking mechanics.
- **`Chara_[Name].c`**: Per-map specific behavior overrides for NPCs or monsters in the area.

## 5. Script-to-Collision Interaction

The core engine (`collision.c`) does not load files directly. It only queries the RAM slots managed by the map streaming system. 

- **Custom Map Application:** If we want secret passages, destructible walls, or doors that open in our custom maps, we do not edit the static `.IPD` collision file at runtime. Instead, our custom map's event scripts (e.g., `events_data.c`) will dynamically toggle the 16-bit wall mask (`s_CollisionState::field_2`), enabling or disabling specific walls in the IPD grid on the fly.
