# Binary Overlay Clarifications

### 1. Viability of a forwards/backwards binary overlay pipeline
**Very viable.** The `s_MapOverlayHdr` is fully documented with a `STATIC_ASSERT_SIZEOF(4172)`, all array bounds are known (`CAMERA_PATH_COUNT_MAX=100`, `COLLISION_TRIGGER_COUNT_MAX=200`, `charaSpawnInfos[2][16]`), and sentinel patterns are identified for each variable-length section. The only lossy parts are funcptrs (stored as PS1 `0x80xxxxxx` addresses — skip or hex-stringify them). Round-tripping is essentially a struct-to-JSON-to-struct problem with no ambiguity in the data sections.

### 2. Meaning of "logical room index"
**Sub-area within the same overlay**, not the IPD chunk. From `Map_RoomIdxGet.h`: `mapRoomIdxGet(posX, posZ)` takes world XZ and returns a small integer (0–~26) by doing AABB/grid comparisons. These index into per-map room-BGM tables (e.g., `Map_RoomBgmInit_*.h`). It's a *logical* room partitioning for audio/BGM purposes, totally independent of which IPD chunk the player stands on.

### 3. Loading screen function specificity
`loadingScreenFuncs` is a **funcptr array** (at `0x18`) — code, not data. Without deeper trace I can't confirm different routines per transition type, but the pointer to `s_MapPoint2d` carries a `loadingScreenId : 3` bitfield, so there are at most **8 distinct loading screen image IDs** selectable per map point. Whether these map to different transition routines or just different artwork is a code question. Not worth deep-diving unless you plan to trigger transitions in tooling.

### 4. Persistent map messages
**Exactly 15 persistent entries (indices 0–14)** are defined in `map_msg_common.h` and `#include`d at the top of every map's `MAP_MESSAGES[]`. These include:
- `[3]` — *"I don't have the map for this place"*
- `[4]` — *"Too dark to look at the map here"*
- `[0/1]` — Yes/No prompts
- `[5–10]` — Item pickup prompts (health drink, ammo, etc.)
- `[11–13]` — Door state messages
- `[14]` — Debug/dev string

Map-specific monologues (Harry's narration, examine text) start at index 15 and vary per overlay.

### 5. Viability of creating an audio player
**Yes, definitely worthwhile.** The system is clean:

- **BGM:** `bgmIdx` (s8 at `0x14`) → indexes `g_BgmTaskLoadCmds[42]` → `SD_Call(cmd)` which loads the `.SEQ` + `.VAB`/`.KDT` pair via Konami's `libsd`. The table has **42 BGM slots** (indices 0–41), with indices 0 and 1 being null/special.
- **Ambient:** `ambientAudioIdx` (u8 at `0x15`) → indexes `g_AmbientVabTaskLoadCmds[40]` → `SD_Call(cmd)` which loads a `.VAB` instrument bank. **40 ambient banks** total. Notable: `map2_s00` has a runtime override that swaps `ambientAudioIdx` between 4 and 11 based on story event flags.
- **Files:** Named `SND/MAP###.VAB` with `.KDT` pairs. Naming is direct by `bgmIdx` value.

A source player that maps `bgmIdx`/`ambientAudioIdx` → `.VAB` → playback would be self-contained. The `SD_Call` command tables are fully reconstructed in the decomp, so you'd know exactly which file each index resolves to.

### 6. Maximum loadable animations and BGM tracks
- **BGM tracks:** Hard limit is **42 slots** (`g_BgmTaskLoadCmds[42]`), with 2 null slots, so ~40 actual tracks.
- **Ambient banks:** **40 slots** (`g_AmbientVabTaskLoadCmds[40]`).
- **Harry map anim overrides** (`harryMapAnimInfos`, `s_AnimInfo*`): Not bounded by a constant in the header — it's a null-terminated pointer to per-map data. The count varies: `map0_s00` has ~18 entries in `g_MapHeaderTable_38`, others have 2–21.
- **Camera paths:** `CAMERA_PATH_COUNT_MAX = 100` (sentinel-terminated in practice, actual count varies per map).

### 7. Purpose of `field_38`
**Your shot in the dark is close but not quite.** Source confirms:

```c
typedef struct {
    s16   status;       // Packed anim status (s_ModelAnim::status)
    s16   status_2;     // Packed anim status
    q3_12 time;         // Fixed-point anim time
    s16   keyframeIdx_6;
} s_UnkStruct3_Mo; // 8 bytes
```

It is **not a pointer cache** — the struct stores **anim playback state snapshots** (status flags + time + keyframe index). The decomp comment says *"Not sure if the time field is actually time — these numbers produce very small non-round values."* Each entry appears to encode a specific anim segment: in `map0_s00` the `status` fields hold packed values like `0x4D4C` (two `s8` packed anim IDs) and the `keyframeIdx` fields span ranges like `0x2A4–0x2A5`. These likely define **map-specific anim segment boundaries or transition states** for Harry's custom anims (complementing `harryMapAnimInfos` at `0x34`). The two fields together form a system: `0x34` describes *which* anim plays; `0x38` describes *how* the playback state machine transitions between keyframe ranges.

### 8. Storage of character spawn data and collectible placements
**Yes for spawns; partially for collectibles:**

- **Character spawns** (`charaSpawnInfos[2][16]`): **Exclusively in the overlay.** `Game_NpcRoomInitSpawn` reads directly from `g_MapOverlayHdr.charaSpawnInfos`.
- **World object poses** (`g_CommonWorldObjectPoses[]`): Also in the overlay's **data segment** (not `s_MapOverlayHdr` itself, but compiled into the same BIN file). These define every pickup item's world position/rotation.
- **Loadable item IDs** (`LOADABLE_INVENTORY_ITEMS[]`): Also in the overlay data segment (null-terminated u8 array), controls which inventory 3D models are preloaded.

There is no secondary source. If an enemy spawn or item placement isn't in the overlay, it doesn't exist for that map.

### 9. Relationship between overlay collision triggers and IPD collision meshes
These are **two completely separate collision systems**:

| | `s_CollisionTrigger` (overlay) | IPD collision geometry |
|---|---|---|
| **Location** | Binary overlay (`MAP*_S**.BIN`) | `.IPD` chunk files |
| **Purpose** | **Elevation steps** — tells movement code when a character steps onto a raised surface (kerb, platform, step) | Full walkable geometry — floor polys, walls, slope normals |
| **Shape** | AABB (posX/Z + sizeX/Z + height in half-metres) | Full 3D polygon mesh with normals |
| **Runtime role** | Patched into character position Y when inside zone | Full raycasting for ground height, wall slides, collision response |

The `unified_cpp_editor`'s collision viewer shows the IPD mesh data. The overlay's `s_CollisionTrigger` array is a *thin layer on top* — it doesn't replace IPD geometry, it *supplements* it by providing discrete step-height hints for geometry that the IPD mesh approximates poorly (e.g., a kerb where the IPD floor poly transitions abruptly). They'd need to be **visualised as separate layers** in any editor — one as polygon soup (IPD), one as a grid of flat AABB boxes (overlay).
