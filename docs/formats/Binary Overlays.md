# PS1 Binary ROM Overlay Packing Specification (Strategy B)

This document specifies the exact 12-byte packed binary structure layouts for **`s_MapPoint`** and **`s_EventTrigger`** required for direct PS1 ROM binary injection and target hardware building.

---

## 1. Waypoint Binary Structure Layout (`s_MapPoint`)
- **Total Size**: 12 bytes
- **Endianness**: Little-Endian (PS1 MIPS)

| Offset | Type | Field Name | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | `int32_t` | `positionX` | Q19.12 fixed-point X coordinate (`worldX * 4096.0f`) |
| `0x04` | `uint8_t` | `paperMapIdx` | Paper map index enum |
| `0x05` | `uint8_t` | `field_4_5` | Reserved / Flag field |
| `0x06` | `uint8_t` | `loadingScreenId` | Loading screen image ID enum |
| `0x07` | `uint8_t` | `unused_4_12` | Padding byte |
| `0x08` | `uint8_t` | `triggerParam0` | Trigger parameter 0 (e.g. AABB extents or angle) |
| `0x09` | `uint8_t` | `triggerParam1` | Trigger parameter 1 (e.g. OBB extents) |
| `0x0A` | `int32_t` | `positionZ` | Q19.12 fixed-point Z coordinate (`worldZ * 4096.0f`) |

### Python Struct Format
```python
import struct
# Format: <i4B2Bi (12 bytes)
binary_bytes = struct.pack('<i4B2Bi',
    int(worldX * 4096.0),
    paperMapIdx_val,
    field_4_5_val,
    loadingScreenId_val,
    unused_4_12_val,
    triggerParam0_val,
    triggerParam1_val,
    int(worldZ * 4096.0)
)
```

---

## 2. Event Trigger Binary Structure Layout (`s_EventTrigger`)
- **Total Size**: 12 bytes
- **Endianness**: Little-Endian (PS1 MIPS)

| Offset | Type | Field Name | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | `uint16_t` | `requiredEventFlag` | Required event flag bitmask |
| `0x02` | `uint16_t` | `disabledEventFlag` | Disabled event flag bitmask |
| `0x04` | `uint8_t` | `triggerType` | Trigger shape enum (`TouchAabb`, `TouchObbFacing`, etc.) |
| `0x05` | `uint8_t` | `activationType` | Activation mode (`Button`, `Exclusive`, etc.) |
| `0x06` | `uint8_t` | `pointOfInterestIdx` | Source waypoint index in active map |
| `0x07` | `uint8_t` | `requiredItemId` | Inventory item requirement enum |
| `0x08` | `uint8_t` | `sysState` | System execution state machine enum |
| `0x09` | `uint8_t` | `eventParam` | Target waypoint index (doors) or callback/message ID |
| `0x0A` | `uint8_t` | `flags_8_13` | Behavior bit flags |
| `0x0B` | `uint8_t` | `sfxPairIdx` | Audio SFX pair index enum |
| `0x0C` | `uint8_t` | `field_8_24` | Reserved byte |
| `0x0D` | `uint8_t` | `mapIdx` | Target destination map index enum (cross-map doors) |

### Python Struct Format
```python
import struct
# Format: <HH8B (12 bytes)
binary_bytes = struct.pack('<HH8B',
    requiredEventFlag_val,
    disabledEventFlag_val,
    triggerTypeValue,
    activationTypeValue,
    pointOfInterestIdx,
    requiredItemIdValue,
    sysStateValue,
    eventParam,
    flags_8_13,
    sfxPairIdxValue,
    field_8_24,
    mapIdxValue
)
```
