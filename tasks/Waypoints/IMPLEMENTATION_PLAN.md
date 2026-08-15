# Room Linkage Editor — Implementation Plan

> [!NOTE]
> All open questions from v1 are **resolved**. This plan is ready for execution.

## Background

The **room linkage editor** is a new sub-task (`tasks/room_linkage_editor`) and a new
viewport tab in `unified_cpp_editor`. Its purpose is to visually edit the two arrays
that together define how the player moves between map sections in Silent Hill 1:

| Source Array | C Struct | File (source) | Role |
|---|---|---|---|
| `mapPoints` | `s_MapPoint2d` (12 bytes) | `map_points.h` | Arrival waypoints — XZ position + angle + loading screen ID |
| `mapEvents` | `s_EventData` (12 bytes) | Inline in overlay header | Trigger volumes — AABB/OBB, flags, which `mapPoints` entry to target, destination map index |

These are parsed from the binary overlay JSON files in `data/workspace/overlays/MAP*_S**/`.
The workspace currently has **no** overlay JSON files (the `overlays/` directory is empty),
so the first deliverable is a **Python extraction script** that parses the existing
decomp C source files and emits per-map JSON. The C++ editor then reads these JSON files.

## Resolved Decisions

| # | Question | Decision |
|---|---|---|
| 1 | **Overlay JSON pipeline** | New Python extraction script first; no C++ JSON parsing. Author mode deferred. |
| 2 | **Trigger AABB bounds** | Exact bounds are embedded in `s_MapPoint2d.triggerParam0/1` — fully verified against `events_main.c`. Precise rendering possible. |
| 3 | **Bidirectional linking** | Inspector has two buttons: **Link** (one-way) and **Link Bidirectionally** (opens second overlay and mutates both). |
| 4 | **Out-of-workspace rendering** | No ghost geometry. Floating billboard label + arrow in a distinct colour (orange). Background geometry shared via pointer to `ViewViewport` batches. |

---

## Trigger Geometry — Source-Verified Formulas

The trigger bounds are **entirely encoded in `s_MapPoint2d`** — verified in events_main.c.

| `triggerType` | Shape | Derivation from `s_MapPoint2d` fields |
|---|---|---|
| `TouchAabb` | AABB | centre = `(positionX, positionZ)`. Half-extents: X = `triggerParam0 × 0.25 m`, Z = `triggerParam1 × 0.25 m` |
| `TouchFacing` | Circle + facing cone | Radius fixed at **0.8 m** (2.8 m FPS). Facing check ±30° |
| `TouchObbFacing` | Rotated rectangle + facing | Angle = `triggerParam0` as Q8 (0–255 → 0°–360°). Half-length = `triggerParam1`. Facing check implicit |
| `TouchObb` | Rotated rectangle | Same OBB geometry as above, no facing requirement. Width fixed at **4.0 m** (from source: `Q12(4.0f)` threshold) |

> [!WARNING]
> `TouchObbFacing` and `TouchObb` use the player's own position as one OBB vertex
> (SAT sweep), not a symmetric box. The visualised OBB is therefore an **approximation**
> of the in-game sweep, centred on `(positionX, positionZ)` with orientation from
> `triggerParam0` and half-extent from `triggerParam1`. This is precise enough for
> editing purposes.

## Event Colour Coding

Based on `sysState` in `s_EventData` (verified from `game.h`):

| Colour | `sysState` values | Meaning |
|---|---|---|
| 🟢 **Green** | `LoadRoom` (6), `LoadOverlay` (5) | Active door / area transition |
| 🟡 **Amber** | `ReadMessage` (7) with door-shaped trigger | Locked / jammed door (shows message instead of loading) |
| ⚪ **Grey** | `EventCallback`, `EventSetFlag`, `EventPlaySound`, `None` | Non-navigational scripted event |
| 🔵 **Blue** | `SaveMenu0/1` | Save point |
| 🟠 **Orange** (label only) | Any, with `mapIdx` pointing to a **different map prefix** | Cross-map transition (e.g. exterior → interior) |

The "jammed door" pattern is: `triggerType = TouchObbFacing`, `activationType = Button`,
`sysState = ReadMessage`, `eventParam = 11`, 12, or 13 (the shared "door locked" message
indices from `map_msg_common.h`). These should render amber and display a lock badge.

---

## Proposed Changes

### New Task File

#### [NEW] `tasks/room_linkage_editor/TASK.md`
Describes the sub-task objective, links back to `unified_cpp_editor` as parent task. (Done)

---

### Phase A — Python Extraction Script (prerequisite)

#### [NEW] `tooling/scripts/extract_overlay.py`

Parses C source files from the decomp (already available in `external/`) and emits
per-map JSON into `data/workspace/overlays/<MAP_KEY>/`.

**Inputs (from decomp source):**
- `src/maps/mapX_sYY/map_points.h` → `s_MapPoint2d[]`
- `src/maps/mapX_sYY/map*_events_data.c` → `s_EventData[]`

**Outputs (per map key, e.g. `MAP6_S04`):**
```
data/workspace/overlays/MAP6_S04/
    map_points.json    ← [{positionX, positionZ, triggerParam0, triggerParam1,
                           paperMapIdx, loadingScreenId, ...}, ...]
    events.json        ← [{triggerType, activationType, pointOfInterestIdx,
                           sysState, eventParam, mapIdx, requiredEventFlag,
                           disabledEventFlag, requiredItemId, ...}, ...]
```

**Strategy:** Regex/tokenizer-based C struct literal parser (not a full C parser).
The decomp source is regular enough (designated-initializer style, one field per line)
that a ~200-line Python script can handle all maps. Enums are resolved to their
integer values using a sidecar enum table extracted from `include/game.h`.

> [!IMPORTANT]
> The script must handle the sentinel correctly:
> - `map_points.h` files have no explicit sentinel — the array length is inferred from
>   the `mapEvents[].pointOfInterestIdx` range and the event data file count.
> - `events_data.c` arrays terminate with `.triggerType = TriggerType_EndOfArray`
>   (`NO_VALUE = -1` as a 4-bit signed field = `0xF`).

---

### Data Layer — Overlay Structs & Loader

#### [MODIFY] `include/core/ipd_structs.h`
Add two new packed C++ structs (matching the canonical decomp layout exactly):

```cpp
// s_MapPoint2d — 12 bytes (matches external/SlickAmogus_silent-hill-decomp)
struct MapPoint2d {
    int32_t  positionX;       // Q19.12 world X
    uint32_t paperMapIdx    : 5;
    uint32_t field_4_5      : 4;
    uint32_t loadingScreenId: 3;
    uint32_t unused_4_12    : 4;
    uint32_t triggerParam0  : 8;  // Q8 arrival angle
    uint32_t triggerParam1  : 8;
    int32_t  positionZ;       // Q19.12 world Z
};
static_assert(sizeof(MapPoint2d) == 12);

// s_EventData — 12 bytes
struct EventData {
    int16_t  requiredEventFlag;
    int16_t  disabledEventFlag;
    uint8_t  triggerType    : 4;  // e_TriggerType
    uint8_t  activationType : 4;
    uint8_t  pointOfInterestIdx;  // index into mapPoints[]
    uint8_t  requiredItemId;
    uint8_t  pad_7;
    uint32_t sysState       : 5;
    uint32_t eventParam     : 8;  // mapEventFuncs index OR mapPoints index
    uint32_t flags_8_13     : 6;
    uint32_t sfxPairIdx     : 5;
    uint32_t field_8_24     : 1;  // "Is on camera rail?"
    uint32_t mapIdx         : 6;  // destination map (e_MapIdx, 0–42)
    uint8_t  pad_c;
};
static_assert(sizeof(EventData) == 12);
```

> [!WARNING]
> The bitfield layout must be verified against actual overlay binary dumps before
> treating writes as correct — bitfield ordering is compiler/platform-dependent.
> On MSVC x64 (the current build target) `uint32_t` bitfields pack LSB-first, which
> matches the PS1 convention for these structs.

#### [NEW] `include/core/OverlayData.h`
C++ structs used internally (no bitfields — easier to edit and serialize):

```cpp
struct WaypointData {
    int     index;          // position in mapPoints[]
    float   worldX, worldZ; // converted from Q19.12
    float   arrivalAngleDeg;// from triggerParam0
    int     loadingScreenId;
    int     paperMapIdx;
    bool    dirty = false;
};

struct LinkData {
    int     index;              // position in mapEvents[]
    int     waypointIdx;        // → WaypointData::index (pointOfInterestIdx)
    int     destMapIdx;         // mapIdx (0–42), or -1 = same map
    int     triggerType;        // e_TriggerType enum value
    int     activationType;
    int     requiredEventFlag;
    int     disabledEventFlag;
    int     requiredItemId;
    bool    dirty = false;
};

struct OverlayMapData {
    std::string mapKey;             // e.g. "MAP0_S00"
    std::vector<WaypointData> waypoints;
    std::vector<LinkData> links;
    bool loaded = false;
    bool dirty  = false;
};
```

#### [NEW] `src/core/OverlayLoader.cpp` / `include/core/OverlayLoader.h`
**Responsibility:** Load and save overlay JSON (`map_points.json`, `events.json`) from
`data/workspace/overlays/<MAP_KEY>/`. Provides `OverlayMapData` to the viewport.

Key functions:
- `Load(mapKey) → OverlayMapData` — reads JSON, returns parsed struct (empty if not found)
- `Save(mapKey, data)` — serializes back to JSON
- `GetMapKeyForChunk(chunkName) → string` — derives `MAP0_S00`-style key from IPD filenames

> [!NOTE]
> The editor uses a hand-written minimal JSON reader — no third-party JSON lib needed.
> The JSON schema is flat arrays of flat objects (no nesting beyond one level), so
> a ~150-line custom reader/writer suffices. Check `libs/` for `nlohmann/json` — if
> it exists, prefer it; otherwise use the custom implementation.

---

### Viewport — `EventViewport`

This follows the same pattern as `CollisionViewport`: inherits `Viewport3DBase`, syncs
via `ViewportSync`, renders an overlay layer on top of the visual geometry.

#### [NEW] `include/viewport/EventViewport.h`
```
class EventViewport : public Viewport3DBase
```

Key members:
- `m_overlay: OverlayMapData` — currently active overlay (one per session)
- `m_selectedWaypointIdx: int` — -1 = none
- `m_selectedLinkIdx: int` — -1 = none
- `m_editMode: enum { None, MoveWaypoint, ReassignDest }`

Public API:
- `LoadOverlay(mapKey)` — calls `OverlayLoader::Load`
- `LoadChunk(ParsedChunk&)` — syncs visual geometry so doors appear in context
- `GetOverlay() → OverlayMapData&`

#### [NEW] `src/viewport/EventViewport.cpp`

**`DrawScene()` rendering layers (drawn in order):**

1. **Background geometry** — `EventViewport` holds a **`const std::vector<LoadedChunk>*`
   pointer** to `ViewViewport`'s chunk list (set by `ViewportSync`). Batches are drawn
   read-only at reduced opacity (rlgl colour multiplier). Zero extra VRAM — no duplication.
   Out-of-workspace chunks are simply absent (no ghost rendering needed).
2. **Waypoint pins** — for each `WaypointData`:
   - Rendered as a vertical spike + disc (`DrawCylinder` + `DrawSphere`).
   - Color coding per attached `LinkData.sysState`:
     - 🟢 Green = `LoadRoom` / `LoadOverlay` door
     - 🟡 Amber = `ReadMessage` at a locked-door-message index (11–13)
     - ⚪ Grey = scripted (non-navigational) event
     - 🔵 Blue = save point
     - 🟠 Orange billboard label = destination is a **different map prefix** (cross-map)
   - Orphaned waypoints (no `LinkData` references it) rendered in red with a `!` badge.
   - A billboard label (ImGui-space overlay projected from 3D) shows `WP[N] → MAP_KEY`.
   - Out-of-workspace cross-map destinations: orange floating arrow + label at a
     **direction-only** position (not a rendered 3D point, since the chunk isn't loaded).
3. **Link trigger volumes** — for each `LinkData`, rendered precisely from `WaypointData`
   geometry (all formulas source-verified):
   - **`TouchAabb`**: `DrawCubeWiresV()` — axis-aligned box, extents from `triggerParam0/1 × 0.25m`
   - **`TouchFacing`**: `DrawCircle3D()` — circle of radius 0.8 m at waypoint XZ, with a 60° cone arc
   - **`TouchObbFacing`** / **`TouchObb`**: `DrawCubeWiresV()` with rotation transform
     applied via `rlPushMatrix` / `rlRotatef(angle, 0, 1, 0)` before draw call.
   - Colour matches the waypoint pin colour. Selected link: bright yellow wireframe.
   - Lock badge (padlock icon via ImGui overlay) on amber events.
4. **Direction arrows** — `DrawLine3D` from each door trigger center to its target
   waypoint. Animated (scrolling dash or pulse via `GetTime()`).

**`HandlePicking(Ray ray)` — clicking in the 3D viewport:**
- Ray-sphere test against each waypoint pin (0.5 world unit radius).
- Ray-box test against each link trigger AABB.
- Sets `m_selectedWaypointIdx` / `m_selectedLinkIdx`.
- Holding `G` + mouse drag while a waypoint is selected → `MoveWaypoint` mode
  (project ray onto Y=0 plane to derive new XZ).

**`DrawToolbar()` (ImGui, inside the panel):**
- Layer toggles: `[x] Waypoints  [x] Links  [x] Direction Arrows`
- `[+ Add Waypoint]` → appends a new `WaypointData` at camera target XZ.
- `[+ Add Link]` → enters "pick link mode" where the user clicks a waypoint pin to
  associate a new `LinkData`.
- `[Delete Selected]` → removes selected waypoint or link (with guard: warn if other
  links reference it).

---

### Inspector Panel — `LinkageInspector`

A separate ImGui panel (not a viewport), docked in the right panel alongside the
scene outliner.

#### [NEW] `include/core/LinkageInspector.h` / `src/core/LinkageInspector.cpp`

Displays editable fields for the currently selected waypoint or link:

**Waypoint inspector:**
```
[Waypoint #N]
  Position X:  [ -123.5  ]   Position Z:  [ 456.25  ]
  Arrival Angle (deg): [ 180° ]
  Loading Screen ID:  [  2  ]  (dropdown: "Cafe", "School", ...)
  Paper Map Index:    [  3  ]
  [Move in Viewport]  [Delete]
```

**Link/Event inspector:**
```
[Link #N → Waypoint #M]
  Sys State:       [ LoadRoom  ▼ ]  (dropdown of all 16 SysState values)
  Destination Map: [ MAP0_S01  ▼ ]  (dropdown of all 43 e_MapIdx names)
                   [⚠ Different map type — cross-map transition]
  Trigger Type:    [ TouchObbFacing  ▼ ]
  Trigger Shape:   [Angle: 180°  Half-ext: 2.0m]  ← derived from triggerParam0/1
  Activation:      [ Button  ▼ ]
  Event Param:     [  7  ]  (message ID / func index / mapPoints index)
  SFX Pair:        [ SfxPairIdx_23  ▼ ]
  Required Flag:   [  0x0000  ]
  Disabled Flag:   [  0x0000  ]
  Required Item:   [ None  ▼ ]
  [Reassign Waypoint]  [Delete]

  -- Linking --
  [Link ↔ MAP0_S01]    [Link Bidirectionally ↔↔ MAP0_S01]
```

**"Link Bidirectionally"** opens the second map's overlay JSON (if present in the
workspace), appends a matching `LinkData`/`WaypointData` pair pointing back, and
saves both JSONs. If the target overlay JSON is not found, an error banner explains
that the reverse link must be authored manually once that overlay is extracted.

The `Destination Map` dropdown lists all 43 `e_MapIdx` symbolic names (resolved from
`include/game.h`/`map.h`) even if not loaded, enabling authoring for any map.

---

### ViewportSync Integration

#### [MODIFY] `include/core/ViewportSync.h` / `src/core/ViewportSync.cpp`

Add `EventViewport` as a 4th sync target:
```cpp
void Update(ChunkManager&, ViewViewport&, CollisionViewport&, EditViewport&, EventViewport&);
void ForceReloadChunk(..., EventViewport&);
```

`EventViewport` does **not** build its own GPU geometry batches. Instead, on init it
receives a `const std::vector<LoadedChunk>* m_sharedChunks` pointer set by
`ViewportSync`. `DrawScene()` iterates this read-only list and re-issues the same
`DrawMesh()` calls at reduced opacity. Out-of-workspace chunks simply have no entry
in the list — no ghost geometry rendered.

> [!IMPORTANT]
> The shared pointer is valid only while `ViewViewport` is alive and its internal
> vector has not reallocated. `ViewportSync` must re-assign the pointer after any
> `ViewViewport::LoadChunk()` / `UnloadAll()` call (same pattern already used for
> camera-state sharing).

---

### ARCHITECTURE.md Update

#### [MODIFY] `tasks/unified_cpp_editor/ARCHITECTURE.md`

Add entries to Section 3 (Rendering & Viewports) and Section 4 (Tooling):

**Section 3 addition:**
| `src/viewport/EventViewport.cpp` | **Door Links & Waypoints**. Renders waypoint pins, link trigger volumes, and direction splines from `OverlayMapData`. Handles 3D picking for waypoint selection and drag-move. | `class EventViewport`, `LoadOverlay()`, `DrawScene()` |

**Section 4 addition:**
| `src/core/OverlayLoader.cpp` | **Overlay JSON I/O**. Reads/writes `map_points.json` and `events.json` from `data/workspace/overlays/<MAP_KEY>/`. Converts Q19.12 fixed-point to float and back. | `class OverlayLoader`, `Load()`, `Save()` |
| `src/core/LinkageInspector.cpp` | **Linkage Inspector UI**. ImGui inspector for the selected waypoint or link event. Provides dropdowns for all 43 map destinations including out-of-workspace maps. | `class LinkageInspector`, `Draw()` |

---

### New Task File

#### [NEW] `tasks/room_linkage_editor/TASK.md`

```markdown
# Task: Room Linkage Editor

**Parent task:** `tasks/unified_cpp_editor`  
**Status:** Planning  
**Phase:** Phase 3 (Door Links & Waypoint Editing) per PLANNED_FEATURES.md

## Objective
Implement an interactive viewport and inspector for viewing and editing the door
waypoints (`map_points.h` / `s_MapPoint2d`) and event triggers (`mapEvents` /
`s_EventData`) that link map sections together. Allows the user to:
- Visualize waypoint arrival points and door trigger volumes in the 3D viewport
- Edit waypoint positions, arrival angles, and loading screen IDs
- Reassign which `s_MapPoint2d` a door trigger targets
- Change the destination map (`mapIdx`) including maps not in the workspace
- Add or delete waypoints and event links
- Serialize edits back to `data/workspace/overlays/<MAP_KEY>/map_points.json` and `events.json`

## Prerequisite
The `data/workspace/overlays/` directory must contain at least one overlay extracted
from a real BIN/DLL. A Python extraction script (`tooling/scripts/extract_overlay.py`)
may need to be authored first.

## Files
- `include/core/OverlayData.h`
- `include/core/OverlayLoader.h` / `src/core/OverlayLoader.cpp`
- `include/viewport/EventViewport.h` / `src/viewport/EventViewport.cpp`
- `include/core/LinkageInspector.h` / `src/core/LinkageInspector.cpp`
- `tasks/unified_cpp_editor/ARCHITECTURE.md` (updated)
- `tasks/unified_cpp_editor/PLANNED_FEATURES.md` (Phase 3 check)
```

---

## Phased Delivery

| Phase | Scope | Output |
|---|---|---|
| **A — Python extractor** | `tooling/scripts/extract_overlay.py` | Populates `data/workspace/overlays/` with per-map JSON |
| **B — Data layer** | `OverlayData.h`, `OverlayLoader.cpp` | Can load/save overlay JSON from C++ |
| **C — Read-only viewport** | `EventViewport.cpp`; `ViewportSync` wired | Coloured pins + shaped trigger volumes visible; no editing |
| **D — Waypoint editing** | Drag-move in 3D, `LinkageInspector` panel | Full waypoint CRUD |
| **E — Link editing** | Event CRUD, Link / Link Bidirectionally, all 43 maps in dropdown | Full link CRUD with bidirectional support |
| **F — Write-back** | `OverlayLoader::Save()` → JSON | Round-trip serialization |

---

## Verification Plan

### Automated
- No unit test framework in the project; verification is manual.

### Manual Verification
1. Load a chunk that has known overlay data (e.g. `MAP0_S00`) → waypoints appear at
   correct XZ (verified against decomp source `map_points.h` values).
2. Click a waypoint in the 3D viewport → it highlights and inspector populates.
3. Drag waypoint to new position → position updates in inspector in real-time.
4. Change destination map in inspector to an out-of-workspace map → UI shows warning
   badge; waypoint arrow label changes; no crash.
5. Save → `map_points.json` updated; reload → data is round-tripped correctly.
6. Build compiles cleanly (`cmake --build build`).
