# Binary Overlay Analysis — Research Notes

These notes consolidate everything learned about SH1 map overlay BIN files across two
research sessions (2026-07-31). All struct definitions are verified against the canonical
source in `external/SlickAmogus_silent-hill-decomp`.

---

## 1. What Is a Binary Overlay?

Each `VIN/MAP*_S**.BIN` file is a compiled PS1 executable overlay — one per map section
(43 total, indexed by `e_MapIdx` in `map.h`). On PS1, it is loaded into a fixed RAM
address and the engine calls through function pointers embedded in its header struct.
On the PC port, each BIN becomes a Windows DLL (`build/maps/map*.dll`).

The overlay is the "director" for the map: it holds no collision geometry (that lives in
`.IPD` chunks) and no visual geometry (`.PLM`/`.TIM`). Instead, it contains all the
per-map *logic and layout data* — camera paths, event zones, spawn coordinates, audio
indices, blood decals, water particles, and more — as static C-struct arrays that the
engine reads at load time.

**Source of truth:** `include/bodyprog/map/map.h` — `s_MapOverlayHdr` struct definition.
Total struct size: **4172 bytes** (`STATIC_ASSERT_SIZEOF(s_MapOverlayHdr, 4172)`).

---

## 2. Full Component Map (`s_MapOverlayHdr`)

The table below lists every field in the struct, ordered by byte offset, along with its
type, size, and meaning. Fields marked **[OPAQUE]** are partially decoded; fields marked
**[JSON-READY]** are flat enough to serialise immediately.

### 2a. Scalar / Pointer Header Fields (offsets 0x0–0x193)

| Offset | Field | Type | Notes |
|--------|-------|------|-------|
| `0x00` | `mapInfo` | `s_MapInfo*` | Points to a global `MAP_INFOS[e_MapType]` record containing the map's 3-letter chunk tag (e.g. `"THR"`, `"ER"`), flag for active chunk count, and global PLM file index. **[POINTER — do not embed]** |
| `0x04` | `mapRoomIdxGet` | funcptr | Determines which logical room index the player is in given X/Y. **[CODE]** |
| `0x0C` | `func_C` | funcptr | Unknown return value funcptr. **[CODE]** |
| `0x10` | `bgmEvent` | funcptr | Per-frame BGM callback, called by `Bgm_TrackUpdate`. **[CODE]** |
| `0x14` | `bgmIdx` | `s8` | Which SEQ track index to load for this map. Indexes into `g_BgmTaskLoadCmds[]`. **[JSON-READY]** |
| `0x15` | `ambientAudioIdx` | `u8` | Which ambient instrument bank (`.VAB`) to load. Indexes `g_AmbientVabTaskLoadCmds[]`. **[JSON-READY]** |
| `0x16` | `field_16` | `s8` | Ambient tint + draw distance. `3` = night mode; `2` = hallway intro tint. **[JSON-READY]** |
| `0x17` | `field_17` | `s8` | Weather type: rain, heavy rain, or snow. **[JSON-READY]** |
| `0x18` | `loadingScreenFuncs` | funcptr array* | Array of loading screen transition functions. **[CODE]** |
| `0x1C` | `mapPoints` | `s_MapPoint2d*` | Pointer to the map's POI coordinate array. **[POINTER → see §2b]** |
| `0x20` | `mapEventFuncs` | funcptr array* | Array of event callback functions (indexed by `s_EventData.eventParam`). **[CODE]** |
| `0x24` | `mapEvents` | `s_EventData*` | Pointer to the map's event trigger array. **[POINTER → see §2c]** |
| `0x28` | `npcBoneCoordBuffer` | `GsCOORDINATE2*` | Always set to `g_SysWork.npcBoneCoordBuffer`; forms the NPC bone ring buffer. **[POINTER — do not embed]** |
| `0x2C` | `loadableItems` | `u8*` | Null-terminated list of item IDs whose 3D models the inventory can load. **[POINTER → see §2e]** |
| `0x30` | `mapMessages` | `const char**` | Array of string pointers for in-map dialogue/examine text (`MapMsg`). **[CODE/TEXT]** |
| `0x34` | `harryMapAnimInfos` | `s_AnimInfo*` | Map-specific Harry animation overrides (for anim indices 38+). References `.ANM` file data. **[POINTER]** |
| `0x38` | `field_38` | `s_UnkStruct3_Mo*` | Array of ~40 packed anim status + time records. Purpose unclear. **[OPAQUE]** |
| `0x3C–0x44` | `initWorldObjects`, `updateWorldObjects`, `func_44` | funcptrs | World object initialisation and per-frame update callbacks. **[CODE]** |
| `0x48` | `npcSpawnEvent` | funcptr | NPC spawn event callback. **[CODE]** |
| `0x4C` | `unkTable1_4C` | `s_MapHdr_field_4C*` | Array of 20-byte structs; likely enemy attack displacement / pushback data. **[OPAQUE]** |
| `0x50` | `unkTable1Count_50` | `s16` | Count of entries in `unkTable1_4C`. **[JSON-READY]** |
| `0x54` | `bloodSplats` | `s_BloodSplat*` | Array of blood decal index records (`s_BloodSplat = { s16 field_0 }`). **[POINTER → simple array]** |
| `0x58` | `bloodSplatCount` | `s16` | Count. **[JSON-READY]** |
| `0x5C` | `field_5C` | `s_MapOverlayHdr_5C*` | 40-byte per-map struct with angles, timers, fields. Partially decoded. **[OPAQUE]** |
| `0x60–0xB7` | Various `func_*` | funcptrs | Per-map visual effect callbacks (fog, particles, water, special geometry draws). Present only on specific maps. **[CODE]** |
| `0x7C` | `field_7C` | `s_MapOverlayHdr_7C*` | 32-byte struct; only on `map1_s01`, `map6_s04`. **[OPAQUE / MAP-SPECIFIC]** |
| `0x94` | `field_94` | `s_MapOverlayHdr_94*` | Water particle emitter struct (~120 bytes); only on `map1_s02`, `map1_s03`. **[OPAQUE / MAP-SPECIFIC]** |
| `0xB8–0xCC` | Player anim hooks | funcptrs | Player anim lock/unlock/freeze callbacks — all wired to the same engine functions. **[CODE]** |
| `0x168` | `particlesUpdate` | funcptr | Per-frame particle update callback. **[CODE]** |
| `0x16C` | `enviromentSet` | funcptr | Sets ambient tint and weather. **[CODE]** |
| `0x178–0x180` | Particle draw funcptrs | funcptrs | Hyper Blaster beam, standard beam, particle sound stop. **[CODE]** |
| `0x184` | `windSpeedX` | `q19_12*` | Wind speed X (Q19.12 fixed-point). Some maps only. |
| `0x188` | `windSpeedZ` | `q19_12*` | Wind speed Z. |
| `0x18C` | `data_18C` | `s32*` | Unknown pointer, particle-related. **[OPAQUE]** |
| `0x190` | `data_190` | `s32*` | Unknown pointer, particle-related. **[OPAQUE]** |

### 2b. Character AI Function Table (offset 0x194)

```
charaUpdateFuncs[Chara_Count]   — funcptr per e_CharaId
```
A sparse array of AI update function pointers, one slot per character ID. Slots for
characters not appearing in this map are `NULL`. Encodes which enemy types are "hosted"
by this overlay. **[CODE — but the NULL/non-NULL pattern is JSON-serialisable as a bitmask
or list of hosted `e_CharaId` values.]**

### 2c. Character Spawn Data (offsets 0x248–0x3CB)

```c
/* 0x248 */ s8       charaGroupIds[CHARA_GROUP_COUNT]; // 4 bytes — e_CharaId group defaults
/* 0x24C */ s_SpawnInfo charaSpawnInfos[2][16];         // 32 slots × 12 bytes = 384 bytes
```

`s_SpawnInfo` (12 bytes, `STATIC_ASSERT_SIZEOF` confirmed):
```c
typedef struct _SpawnInfo {
    /* 0x0 */ q19_12 positionX;          // Q19.12 world X
    /* 0x4 */ s8     charaId;            // e_CharaId
    /* 0x5 */ q0_8   rotationY;          // 0–255 mapped to 0°–360°
    /* 0x6 */ s8     flags;              // e_SpawnFlags
    /* 0x7 */ s32    gameDifficultyMin:4;// e_GameDifficulty minimum
    /* 0x8 */ q19_12 positionZ;          // Q19.12 world Z
} s_SpawnInfo; // 12 bytes
```

- Two banks of 16 slots each (`charaSpawnInfos[0]` and `[1]`).
- `charaGroupIds[0]` is the fallback `charaId` for bank 0 when `SpawnInfo.charaId == Chara_None`.
- `flags == SpawnFlags_None` marks an unused slot.
- **[JSON-READY]** — all fields are integers or enums.

### 2d. Camera Paths (offset 0x3CC)

```c
/* 0x3CC */ VC_ROAD_DATA cameraPaths[CAMERA_PATH_COUNT_MAX]; // 100 × 24 bytes = 2400 bytes
```

`VC_ROAD_DATA` (24 bytes, `STATIC_ASSERT_SIZEOF` confirmed):
```c
typedef struct _VC_ROAD_DATA {
    /* 0x0  */ VC_LIMIT_AREA lim_sw;       // 8 bytes — switch zone AABB (q11_4 min/maxX, min/maxZ)
    /* 0x8  */ VC_LIMIT_AREA lim_rd;       // 8 bytes — road zone AABB
    /* 0x10 */ VC_ROAD_FLAGS flags    : 8; // camera path behaviour flags
    /* 0x11 */ VC_AREA_SIZE_TYPE area_size_type : 2;
    /* 0x11 */ VC_ROAD_TYPE  rd_type  : 3; // path type (fixed, track, etc.)
    /* 0x11 */ u32           mv_y_type: 3; // VC_CAM_MV_TYPE
    /* 0x12 */ q27_4 lim_rd_max_hy   : 8; // max camera Y in road zone
    /* 0x13 */ q27_4 lim_rd_min_hy   : 8; // min camera Y
    /* 0x14 */ q27_4 ofs_watch_hy    : 8; // look-at Y offset
    /* 0x14 */ u32   lens_flare      : 4; // e_LensFlareType
    /* 0x14 */ s16   cam_mv_type     : 4; // VC_CAM_MV_TYPE
    /* 0x16 */ q0_7  fix_ang_x;           // fixed camera pitch angle
    /* 0x17 */ q0_7  fix_ang_y;           // fixed camera yaw angle
} VC_ROAD_DATA; // 24 bytes
```

The array is sentinel-terminated (likely by a zeroed entry or `isEndOfArray` pattern;
confirm via source). **[JSON-READY]** — all fields are fixed-point integers with known
scale factors.

> **Note:** In SH2, camera paths live in standalone `.cam` files. In SH1 they are baked
> directly into the overlay header. This is why the struct comment in `structs.h` says
> "In SH2, the `.cam` files contain this struct, while in SH1 this is part of `s_MapOverlayHdr`."

### 2e. Collision Triggers / Floor Step Zones (offset 0xD2C)

```c
/* 0xD2C */ s_CollisionTrigger collisionTriggers[COLLISION_TRIGGER_COUNT_MAX]; // 200 × 4 bytes = 800 bytes
```

`s_CollisionTrigger` (4 bytes, `STATIC_ASSERT_SIZEOF` confirmed):
```c
typedef struct _CollisionTrigger {
    /* 0x0+0  */ u8  isEndOfArray : 1;  // sentinel
    /* 0x0+1  */ s32 positionX    : 10; // world X origin, whole metres, signed
    /* 0x0+11 */ s32 positionZ    : 10; // world Z origin, whole metres, signed
    /* 0x0+21 */ u32 sizeX        : 4;  // width in whole metres
    /* 0x0+25 */ u32 sizeZ        : 4;  // depth in whole metres
    /* 0x0+29 */ u32 height       : 3;  // elevation in half-metre steps (0–7 → 0.0–3.5 m)
} s_CollisionTrigger; // 4 bytes
```

Despite what older documentation called these ("trigger zones"), **they are not event
triggers**. They are purely an elevation system — axis-aligned bounding boxes that tell
the movement code when a character steps onto a raised floor (platform, kerb, step).
The `±16 m` extended sensing radius is runtime-only and does not need to be serialised.

Coordinate ranges: positionX/Z ∈ [−512, 511] m; sizeX/Z ∈ [0, 15] m; height ∈ [0, 7].

**[JSON-READY]** — all fields are bitfield integers with clear physical interpretations.

### 2f. Map Points (via pointer at 0x1C)

`s_MapPoint2d` (12 bytes):
```c
typedef struct _MapPoint2d {
    /* 0x0    */ q19_12 positionX;
    /* 0x4+0  */ u32    paperMapIdx     : 5; // e_PaperMapIdx
    /* 0x4+5  */ u32    field_4_5       : 4;
    /* 0x4+9  */ u32    loadingScreenId : 3; // e_LoadingScreenId
    /* 0x4+12 */ u32    unused_4_12     : 4; // always 0
    /* 0x4+16 */ q24_8  triggerParam0   : 8; // usually a Q8 angle
    /* 0x4+24 */ u32    triggerParam1   : 8;
    /* 0x8    */ q19_12 positionZ;
} s_MapPoint2d; // 12 bytes
```

These are **not** simple XYZ coordinates. They encode a world XZ position plus bitpacked
metadata: which paper map thumbnail to show, which loading screen image to use, and two
trigger parameters (e.g., arrival facing angle). Used by `s_EventData` to locate door
thresholds and examination points. **[JSON-READY]** — all fields are integers.

### 2g. Event Triggers (via pointer at 0x24)

`s_EventData` (12 bytes):
```c
typedef struct _EventData {
    /* 0x0   */ s16 requiredEventFlag;      // flag that must be SET for this event to fire
    /* 0x2   */ s16 disabledEventFlag;      // flag that must be CLEAR
    /* 0x4+0 */ s8  triggerType    : 4;     // e_TriggerType (TouchAabb/TouchFacing/TouchObb...)
    /* 0x4+4 */ u8  activationType : 4;     // e_TriggerActivationType (None/Button/Item...)
    /* 0x5   */ u8  pointOfInterestIdx;     // index into mapPoints[]
    /* 0x6   */ u8  requiredItemId;         // e_InvItemId the player must be holding
    /* 0x7   */ u8  __pad_7;
    /* 0x8+0 */ u32 sysState        : 5;    // e_SysState to enter when triggered
    /* 0x8+5 */ u32 eventParam      : 8;    // msg ID / sfx ID / mapEventFuncs index / mapPoints index
    /* 0x8+8 */ u32 flags_8_13      : 6;    // e_EventDataUnkState (freeze world, cutscene flags)
    /* 0x8+19*/ u32 sfxPairIdx_8_19 : 5;   // e_SfxPairIdx
    /* 0x8+19*/ u32 field_8_24      : 1;    // "Is on camera rail?"
    /* 0x8+24*/ u32 mapIdx          : 6;    // destination map index for area-load events
} s_EventData; // 12 bytes
```

`TriggerType_EndOfArray` (`NO_VALUE`) terminates the array. Trigger types include:
- `TouchAabb` — player enters an axis-aligned bounding box
- `TouchFacing` — player enters AABB AND is facing toward it
- `TouchObbFacing` — player enters oriented bounding box AND is facing
- `TouchObb` — player enters OBB (no facing requirement)

**[JSON-READY]** with enum name mapping.

### 2h. World Object Poses (embedded in map DLL / overlay data segment)

`g_CommonWorldObjectPoses[]` — **not inside `s_MapOverlayHdr`** but in the overlay's data
segment, extracted separately.

`s_WorldObjectPose` (20 bytes):
```c
typedef struct {
    VECTOR3  position;    // 3 × s32, 12 bytes (Q19.12 world coords)
    SVECTOR3 rotation_C;  // 3 × s16, 6 bytes  (Q3.12 angles)
    // 2 bytes implicit padding
} s_WorldObjectPose; // 20 bytes
```

These define the exact world position and rotation of every pickup item (ammo boxes,
health drinks, keys, notes). The per-map header C file indexes into this array to place
each item. **[JSON-READY]** — extracted by the decomp's `extract_map_data.py` tool.

### 2i. Loadable Inventory Items (embedded in overlay data segment)

`LOADABLE_INVENTORY_ITEMS[]` — null-terminated `u8[]` of `e_InvItemId` values.

Determines which item IDs can have their 3D inventory models (`.TMD` pack, e.g.,
`FILE_ITEM_IT_001_TMD`) loaded while the player is in this map. The inventory screen only
shows 3D models for items whose ID is in this list. **[JSON-READY]** — simple integer array.

---

## 3. Map Types & IPD Chunk Loading Architecture

A crucial discovery regarding binary overlays and `.IPD` chunks is that **the binary overlay does not contain a hardcoded list of available IPD chunk files**.

Instead, the connection between a map overlay and its IPD chunks is dynamic and relies on the CD filesystem:
1. The overlay's `mapInfo` points to a `MAP_INFOS[e_MapType]` struct in `map_info.c`.
2. This struct defines a 3-character `mapTag` (e.g., `"THR"` for alleys, `"ER"` for the school/bus interior).
3. During map load, `Map_MakeIpdGrid` scans the global `g_FileTable` (the entire filesystem) for all `FileType_Ipd` files whose filenames begin with that `mapTag`.
4. It parses the remaining 4 characters of matching filenames as X and Z grid coordinates (in hexadecimal, two's complement). For example, `THRFF01.IPD` translates to X=-1 (FF), Z=1 (01).
5. It slots the file index into a 19x16 spatial lookup grid (`s_MapTerrain.chunkGrid`).

At runtime, the engine uses the player's position to stream chunks into a 4-slot LRU cache (`s_MapTerrain.activeChunks[4]`).
Different maps that use the same `e_MapType` (like `map0_s00` and `map0_s01` both using `MapType_THR`) inherently share the exact same global pool of IPD chunks. 

When transitioning to a radically different area—such as entering the interior of the school bus from the streets—an event trigger fires a complete map reload (`map0_s00` unloads, `map0_s02` loads). `map0_s02` uses `MapType_ER` instead of `MapType_THR`, causing the grid to populate with `ER####.IPD` chunks rather than `THR####.IPD`.

---

## 4. Referenced File Types (Not Embedded in the BIN)

The overlay does not embed these directly but controls which ones are loaded:

| Extension | Name | Role | Location |
|-----------|------|------|----------|
| `.IPD` | Map chunk | Collision geometry + embedded PLM | `BG/` subdirs (streamed by `g_Map`) |
| `.PLM` | PlayStation Model | Static and dynamic geometry | `BG/` subdirs |
| `.TIM` | PlayStation Image | Texture pages (CLUT + pixel data) | `BG/` subdirs |
| `.ANM` | Animation | Harry's and enemy skeleton animations | `ANIM/HB_M*S**.ANM` etc. |
| `.DMS` | Demo/Cutscene Script | In-engine pre-rendered sequences | `ANIM/*.DMS` |
| `.VAB` | Voice Audio Bank | SPU instrument samples for BGM/SFX | `SND/MAP***.VAB` |
| `.KDT` | Key Data Table | SPU voice-slot allocation for a VAB | `SND/*.KDT` (paired with VAB) |
| `.TMD` | PlayStation Model Data | Inventory item 3D models | `ITEM/` (loaded on demand) |
| `.SEQ` | MIDI Sequence | BGM track data (Konami libsd format) | Loaded from SPU via `SD_Call` |

**Per-map VAB naming convention:** `SND/MAP###.VAB` where `###` maps to the `bgmIdx`
value (e.g., `bgmIdx = 0` → `MAP000.VAB`; `bgmIdx = 100` → `MAP100.VAB`).

**`.ANM` naming convention:** `ANIM/HB_M{mapN}S{secNN}.ANM` — directly mirrors the map
section index (e.g., `map0_s01` → `HB_M0S01.ANM`).

**`.DMS` naming convention:** Named by scene/location (e.g., `NURSE1.DMS`, `LAST3.DMS`,
`ENDAA.DMS`). Not all map overlays reference DMS files.

---

## 5. Accuracy Corrections to Earlier Notes

1. **Naming:** What `trigger_zones.md` called `s_TriggerZone` is canonically `s_CollisionTrigger`
   in the main source (`trigger.h`). Both refer to the same 4-byte bitfield struct.
2. **Map Points are not simple XYZ.** They are 12-byte packed structs (`s_MapPoint2d`)
   with loading screen IDs, paper map indices, and trigger parameters alongside position.
3. **Events are a separate concern from Map Points.** `s_EventData` references `s_MapPoint2d`
   via index — they are two distinct arrays with different parsers.
4. **`.SEQ` files are not embedded.** BGM music is loaded dynamically by `Bgm_TrackUpdate`
   via `SD_Call` using the `bgmIdx` field; the actual SEQ bytes live in SPU memory managed
   by `libsd`, not in the BIN overlay.

---

## 6. Proposed Workspace Output Layout

When parsed, each map overlay should produce a directory under `data/workspace/overlays/`:

```
data/workspace/
    overlays/
        MAP0_S00/
            header.json              ← scalar fields: bgmIdx, ambientAudioIdx, field_16/17, etc.
            camera_paths.json        ← VC_ROAD_DATA[0..N] (sentinel-terminated)
            collision_triggers.json  ← s_CollisionTrigger[0..N] (sentinel-terminated)
            spawns.json              ← charaGroupIds[4] + s_SpawnInfo[2][16]
            map_points.json          ← s_MapPoint2d[] array
            events.json              ← s_EventData[] (TriggerType_EndOfArray-terminated)
            world_objects.json       ← g_CommonWorldObjectPoses[]
            loadable_items.json      ← LOADABLE_INVENTORY_ITEMS[] null-terminated list
            blood_splats.json        ← s_BloodSplat[] (when present)
```

The directory name should match the `e_MapIdx` enum value (i.e., the BIN filename without
extension). This allows tooling to cross-reference the chunk pipeline (`overlays/MAP0_S00/`
vs. chunks loaded by that map's `mapType → MAP_INFOS → plmFileIdx`).

---

## 7. Known Unknowns / Open Questions

- **Load address offset:** The BIN file contains a PS1 executable header before the
  overlay data. The virtual address of `g_MapOverlayHeader_mapN_sNN` needs to be read
  from `configs/USA/maps/sym.mapN_sNN.txt` to compute the correct byte offset into the
  BIN. This is the first thing to verify experimentally.
- **Function pointer slots:** All funcptr fields in the header will read as PS1 RAM
  addresses (0x80xxxxxx) in the raw BIN. The parser should store these as hex strings
  or skip them entirely — they cannot be meaningfully serialised as data.
- **`field_5C`, `field_7C`, `field_94`:** These nested structs remain partially opaque.
  `field_5C` appears on all maps; `field_7C` and `field_94` are map-specific. Their
  fields should be stored as raw byte arrays until further analysis.
- **Camera path termination:** The source says `CAMERA_PATH_COUNT_MAX = 100` but the
  actual sentinel condition is not yet confirmed in the parser source. Needs verification.
- **`s_MapHdr_field_4C` (unkTable1_4C):** The 20-byte struct contains displacement
  offsets, angles, and timers that may relate to enemy pushback. The count
  (`unkTable1Count_50`) is a known scalar; the content is opaque.
