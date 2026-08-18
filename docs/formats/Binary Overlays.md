# Binary Overlay Struct Layouts

This document specifies the binary structure layouts for the two primary data-driven overlay structs: **`s_MapPoint2d`** (waypoints/points of interest) and **`s_EventData`** (event triggers). Both structs are exactly **12 bytes** and use Little-Endian byte order.

For an architectural overview of map overlays, see [`Map Overlay System.md`](../Map Overlay System.md). For the master `s_MapOverlayHdr` component map, see `tasks/BinaryOverlays/notes.md`.

---

## 1. Waypoint Structure (`s_MapPoint2d`)
- **Total Size**: 12 bytes (`STATIC_ASSERT_SIZEOF(s_MapPoint2d, 12)`)
- **Endianness**: Little-Endian (PS1 MIPS)
- **Canonical Source**: `include/bodyprog/map/map.h`

### Byte & Bit Layout

Bytes `0x04`–`0x07` form a single 32-bit unsigned integer containing packed bitfields:

| Byte Offset | Bit Range | Type | Field Name | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0x00`–`0x03` | — | `int32_t` | `positionX` | Q19.12 fixed-point X coordinate (`worldX * 4096.0f`) |
| `0x04`–`0x07` | `[0..4]` (5 bits) | `uint32_t` | `paperMapIdx` | Paper map index enum (`e_PaperMapIdx`) |
| | `[5..8]` (4 bits) | `uint32_t` | `field_4_5` | Reserved / flag field |
| | `[9..11]` (3 bits) | `uint32_t` | `loadingScreenId` | Loading screen image ID enum (`e_LoadingScreenId`) |
| | `[12..15]` (4 bits) | `uint32_t` | `unused_4_12` | Always 0 (padding) |
| | `[16..23]` (8 bits) | `q24_8` | `triggerParam0` | Trigger parameter 0 (e.g. AABB half-extent X, or Q8 facing angle) |
| | `[24..31]` (8 bits) | `uint32_t` | `triggerParam1` | Trigger parameter 1 (e.g. AABB half-extent Z, or OBB half-length) |
| `0x08`–`0x0B` | — | `int32_t` | `positionZ` | Q19.12 fixed-point Z coordinate (`worldZ * 4096.0f`) |

### C / C++ Definition
```c
typedef struct _MapPoint2d {
    /* 0x0 */ q19_12 positionX;
    /* 0x4 */ uint32_t paperMapIdx     : 5;
              uint32_t field_4_5       : 4;
              uint32_t loadingScreenId : 3;
              uint32_t unused_4_12     : 4;
              q24_8    triggerParam0   : 8;
              uint32_t triggerParam1   : 8;
    /* 0x8 */ q19_12 positionZ;
} s_MapPoint2d; // 12 bytes
```

### Python Packing Example
```python
import struct

# Pack middle 32-bit word using bitshifts (0x04-0x07)
flags_word = (
    (paperMapIdx & 0x1F) |
    ((field_4_5 & 0x0F) << 5) |
    ((loadingScreenId & 0x07) << 9) |
    ((unused_4_12 & 0x0F) << 12) |
    ((triggerParam0 & 0xFF) << 16) |
    ((triggerParam1 & 0xFF) << 24)
)

# Format: <iIi (12 bytes: int32, uint32, int32)
binary_bytes = struct.pack('<iIi',
    int(worldX * 4096.0),
    flags_word,
    int(worldZ * 4096.0)
)
```

---

## 2. Event Trigger Structure (`s_EventData`)
- **Total Size**: 12 bytes (`STATIC_ASSERT_SIZEOF(s_EventData, 12)`)
- **Endianness**: Little-Endian (PS1 MIPS)
- **Canonical Source**: `include/bodyprog/map/map.h`

### Byte & Bit Layout

| Byte Offset | Bit Range | Type | Field Name | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0x00`–`0x01` | — | `int16_t` | `requiredEventFlag` | Event flag that must be SET for this event to fire |
| `0x02`–`0x03` | — | `int16_t` | `disabledEventFlag` | Event flag that must be CLEAR (retires trigger once set) |
| `0x04` | `[0..3]` (4 bits) | `int8_t` | `triggerType` | `e_TriggerType` (TouchAabb, TouchFacing, etc.) |
| | `[4..7]` (4 bits) | `uint8_t` | `activationType` | `e_TriggerActivationType` (None, Exclusive, Button, Item) |
| `0x05` | — | `uint8_t` | `pointOfInterestIdx` | Index into the map's `mapPoints[]` array |
| `0x06` | — | `uint8_t` | `requiredItemId` | `e_InvItemId` the player must use |
| `0x07` | — | `uint8_t` | `__pad_7` | Padding byte |
| `0x08`–`0x0B` | `[0..4]` (5 bits) | `uint32_t` | `sysState` | `e_SysState` to enter when triggered |
| | `[5..12]` (8 bits) | `uint32_t` | `eventParam` | Message ID / SFX ID / mapEventFuncs index / mapPoints index |
| | `[13..18]` (6 bits) | `uint32_t` | `flags_8_13` | `e_EventDataUnkState` (freeze world, cutscene flags) |
| | `[19..23]` (5 bits) | `uint32_t` | `sfxPairIdx` | `e_SfxPairIdx` |
| | `[24]` (1 bit) | `uint32_t` | `field_8_24` | "Is on camera rail?" flag |
| | `[25..30]` (6 bits) | `uint32_t` | `mapIdx` | Destination map index for area-load events (0–42) |
| | `[31]` (1 bit) | `uint32_t` | — | Alignment pad |

### C / C++ Definition
```c
typedef struct _EventData {
    /* 0x0 */ int16_t  requiredEventFlag;
    /* 0x2 */ int16_t  disabledEventFlag;
    /* 0x4 */ uint8_t  triggerType    : 4;
              uint8_t  activationType : 4;
    /* 0x5 */ uint8_t  pointOfInterestIdx;
    /* 0x6 */ uint8_t  requiredItemId;
    /* 0x7 */ uint8_t  __pad_7;
    /* 0x8 */ uint32_t sysState       : 5;
              uint32_t eventParam     : 8;
              uint32_t flags_8_13     : 6;
              uint32_t sfxPairIdx     : 5;
              uint32_t field_8_24     : 1;
              uint32_t mapIdx         : 6;
              uint32_t __pad_bit_31   : 1;
} s_EventData; // 12 bytes
```

### Python Packing Example
```python
import struct

# Byte 0x04: triggerType (lower 4 bits) + activationType (upper 4 bits)
byte_04 = (triggerType & 0x0F) | ((activationType & 0x0F) << 4)

# Bytes 0x08-0x0B: 32-bit packed word
word_08 = (
    (sysState & 0x1F) |
    ((eventParam & 0xFF) << 5) |
    ((flags_8_13 & 0x3F) << 13) |
    ((sfxPairIdx & 0x1F) << 19) |
    ((field_8_24 & 0x01) << 24) |
    ((mapIdx & 0x3F) << 25)
)

# Format: <hh4BI (12 bytes: 2 int16, 4 uint8, 1 uint32)
binary_bytes = struct.pack('<hh4BI',
    requiredEventFlag,
    disabledEventFlag,
    byte_04,
    pointOfInterestIdx,
    requiredItemId,
    0, # pad_7
    word_08
)
```

---

## 3. Trigger Types & Geometry

Trigger bounding geometry is **embedded inside `s_MapPoint2d`** and evaluated in `events_main.c`:

| `triggerType` | Shape | Parameters in `s_MapPoint2d` |
| :--- | :--- | :--- |
| `TriggerType_None` | No volume | Event fires on flags alone |
| `TriggerType_TouchAabb` | Axis-Aligned Bounding Box | Half-extents: X = `triggerParam0 × 0.25m`, Z = `triggerParam1 × 0.25m` |
| `TriggerType_TouchFacing` | Circle + Facing Cone | Radius = 0.8m, ±30° cone. `triggerParam0` = arrival angle (Q8) |
| `TriggerType_TouchObbFacing` | Oriented Bounding Box + Facing | `triggerParam0` = angle (Q8), `triggerParam1` = half-length |
| `TriggerType_TouchObb` | Oriented Bounding Box (any facing) | Width fixed at 4.0m, `triggerParam1` = half-length |
| `TriggerType_EndOfArray` | Terminator | Sentinel struct marking end of the array |
