"""
Struct-level definitions for the Silent Hill 1 (PS1) .IPD / .PLM formats.

GROUND TRUTH: github.com/belek666/sh_ipd2obj, main.c + tim.c, read in full
and cross-checked field by field (grepped for every field's usage site, not
assumed from the struct comment). Nothing here is copied from secondary
documentation without being reconciled against that source.

STATUS TAGS (see FieldStatus below) are attached to every field so that
"confirmed" and "hypothesis" never get silently blurred together:

  CONFIRMED    ipd2obj actually dereferences this field, AND/OR its byte
               offset has been independently cross-checked against real
               data (e.g. arithmetic on THR0000.IPD's own conversion log).
  UNUSED       ipd2obj's fread() physically reads this byte range into
               memory (so it is confirmed to exist at this offset, at this
               size) but the tool never dereferences the field afterwards.
               Its *meaning* is therefore an unverified guess (the C
               comment, if any, is preserved as a hypothesis label only).
  HYPOTHESIS   Not present in ipd2obj at all. Existence, offset and size
               are inferred/external and NOT independently confirmed here.
  RUNTIME_ONLY Meaningful only after the game loads the file into RAM
               (e.g. becomes a pointer). Expected to be inert/garbage in
               the on-disk file; not a case of "data we are failing to
               read", just not static content.

All multi-byte fields are little-endian (PS1 CPU is MIPS R3000 in LE mode).
Every struct below needs zero compiler-inserted padding under normal C
alignment rules -- this was checked by hand for each struct (field sizes
sum to a total where every subsequent 4-byte int/short field already lands
on a naturally-aligned offset) and is re-verified programmatically below
via struct.calcsize(). Python's struct module with a '<' prefix also never
inserts alignment padding, so the two independent models agree by
construction, not by luck alone -- but see IPD_OBJ_DATA's `pad` field: the
original struct author had to hand-insert that filler short to keep `tx`
aligned, which is itself evidence the on-disk format really was produced
by a natural-alignment C compiler, not something arbitrarily packed.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import ClassVar


class FieldStatus:
    CONFIRMED = "confirmed"
    UNUSED = "unused"
    HYPOTHESIS = "hypothesis"
    RUNTIME_ONLY = "runtime_only"


# ---------------------------------------------------------------------------
# IPD_FILE_HEADER -- 84 bytes (0x54), confirmed size matches ipd2obj's own
# comment ("all offsets += sizeof(IPD_FILE_HEADER) (0x54)") and matches the
# hand-summed field sizes below with zero padding.
# ---------------------------------------------------------------------------
IPD_FILE_HEADER_FORMAT = "<BBbbiBBBB8sii52si"
IPD_FILE_HEADER_FIELDS = [
    # (name, status, note)
    ("id", FieldStatus.CONFIRMED, "0x14 for a valid .IPD file"),
    ("flag", FieldStatus.UNUSED, "'0x01 when file is loaded by game' per comment"),
    ("x_pos", FieldStatus.CONFIRMED, "map x coordinate, matches filename"),
    ("y_pos", FieldStatus.CONFIRMED, "map y coordinate, matches filename"),
    ("plm_offset", FieldStatus.CONFIRMED, "offset to embedded PLM section"),
    ("obj_num", FieldStatus.UNUSED, "'number of objects on the list', read but never checked"),
    ("pos_num", FieldStatus.CONFIRMED, "count of IPD_POS_HEADER entries; loop bound"),
    ("unk1_num", FieldStatus.UNUSED, "'drawing distance table size?' -- never dereferenced"),
    ("unk2", FieldStatus.UNUSED, "no hypothesis given in source"),
    ("pad1", FieldStatus.UNUSED, "8 raw bytes, never interpreted"),
    ("obj_name_offset", FieldStatus.CONFIRMED, "offset to IPD_OBJNAME_DATA table"),
    ("obj_data_offset", FieldStatus.CONFIRMED,
     "offset to IPD_POS_HEADER array -- independently reproduced from THR0000's log"),
    ("unk1_data", FieldStatus.UNUSED, "52 raw bytes, 'drawing distance global table?' -- never interpreted"),
    ("unkdata_offset", FieldStatus.UNUSED,
     "points to data that is NEVER READ ('obj indices/drawing order?')"),
]

# ---------------------------------------------------------------------------
# IPD_COLL_HEADER -- 308 bytes (0x134), located at offset 0x54 inside IPD.
# CONFIRMED: Defines 7 distinct collision arrays and their exact counts and 
# absolute pointers. Padding is kept verbatim for exact roundtripping.
# ---------------------------------------------------------------------------
IPD_COLL_HEADER_FORMAT = "<iiBBBBiiiiHBBiHHiii256s"
IPD_COLL_HEADER_FIELDS = [
    ("positionX", FieldStatus.CONFIRMED, "Global collision map x position"),
    ("positionZ", FieldStatus.CONFIRMED, "Global collision map z position"),
    ("splitVertexCount", FieldStatus.CONFIRMED, "Count of split vertices (SVECTOR3)"),
    ("surfaceCount", FieldStatus.CONFIRMED, "Count of surface elements"),
    ("subcellCount", FieldStatus.CONFIRMED, "Count of subcell elements"),
    ("unkBlock3Count", FieldStatus.CONFIRMED, "Count of unknown block 3 elements"),
    ("ptr_splitVertices", FieldStatus.CONFIRMED,
     "Offset to splitVertices array, relative to the start of this collision "
     "header (patched as `raw + (u32)collData` in Ipd_CollisionPtrsInit, "
     "0x8006993C) -- NOT an absolute file offset."),
    ("ptr_surfaces", FieldStatus.CONFIRMED,
     "Offset to surfaces array, relative to the start of this collision header -- see ptr_splitVertices."),
    ("ptr_subcells", FieldStatus.CONFIRMED,
     "Offset to subcells array, relative to the start of this collision header -- see ptr_splitVertices."),
    ("ptr_unkBlock3", FieldStatus.CONFIRMED,
     "Offset to unkBlock3 array, relative to the start of this collision header -- see ptr_splitVertices."),
    ("gridScale", FieldStatus.CONFIRMED, "Grid scale (Usually 0x0200 = 512)"),
    ("gridWidth", FieldStatus.CONFIRMED, "Grid width (20 on files checked so far -- read from disk, don't assume constant)"),
    ("gridHeight", FieldStatus.CONFIRMED, "Grid height (20 on files checked so far -- read from disk, don't assume constant)"),
    ("ptr_grid", FieldStatus.CONFIRMED,
     "Offset to broadphase grid array, relative to the start of this collision header -- see ptr_splitVertices."),
    ("block5Count", FieldStatus.CONFIRMED, "Count of block 5 elements"),
    ("block6Count", FieldStatus.CONFIRMED, "Count of block 6 elements"),
    ("ptr_block5", FieldStatus.CONFIRMED,
     "Offset to block 5 array, relative to the start of this collision header -- see ptr_splitVertices."),
    ("ptr_block6", FieldStatus.CONFIRMED,
     "Offset to block 6 array, relative to the start of this collision header -- see ptr_splitVertices."),
    ("ptr_unk7", FieldStatus.CONFIRMED,
     "Pointer 7 (usually 0). NOTE: unlike the other ptr_* fields, Ipd_CollisionPtrsInit "
     "never patches this one -- do not add header_end to it."),
    ("subcellCheckIdx", FieldStatus.CONFIRMED, "Subcell check index array (256 bytes)"),
]

# ---------------------------------------------------------------------------
# IPD_COLL_SVECTOR -- 6 bytes.
# ---------------------------------------------------------------------------
IPD_COLL_SVECTOR_FORMAT = "<hhh"
IPD_COLL_SVECTOR_FIELDS = [
    ("x", FieldStatus.CONFIRMED, None),
    ("y", FieldStatus.CONFIRMED, None),
    ("z", FieldStatus.CONFIRMED, None),
]

# ---------------------------------------------------------------------------
# IPD_COLL_SURFACE -- 12 bytes.
# ---------------------------------------------------------------------------
IPD_COLL_SURFACE_FORMAT = "<hhhHhh"
IPD_COLL_SURFACE_FIELDS = [
    ("field_0", FieldStatus.CONFIRMED, "Relative X (q7_8)"),
    ("baseGroundHeight", FieldStatus.CONFIRMED, "Base Y (q7_8)"),
    ("field_4", FieldStatus.CONFIRMED, "Relative Z (q7_8)"),
    ("tilt_flags", FieldStatus.CONFIRMED, "Tilt and collision type flags"),
    ("tiltAngleX", FieldStatus.CONFIRMED, "Pitch (q7_8)"),
    ("tiltAngleZ", FieldStatus.CONFIRMED, "Roll (q7_8)"),
]

# ---------------------------------------------------------------------------
# IPD_COLL_SUBCELL -- 10 bytes.
# ---------------------------------------------------------------------------
IPD_COLL_SUBCELL_FORMAT = "<hhhBBBB"
IPD_COLL_SUBCELL_FIELDS = [
    ("field_0", FieldStatus.CONFIRMED, "X and flags (14 bits X, 2 bits ID)"),
    ("field_2", FieldStatus.CONFIRMED, "Y and flags (14 bits Y, 2 bits ID)"),
    ("field_4", FieldStatus.CONFIRMED, "Z (q7_8)"),
    ("splitVertexIdx0", FieldStatus.CONFIRMED,
     "Flat u8 index into splitVertices. Confirmed directly from collision.c's "
     "func_8006B318 (0x8006B318): `collData->splitVertices[subcell->splitVertexIdx1]` / "
     "`[subcell->splitVertexIdx0]` are dereferenced unconditionally. The earlier "
     "67%-OOB result on THR0000 was traced to ipd_to_local_json.py resolving "
     "ptr_subcells/ptr_splitVertices as absolute file offsets instead of offsets "
     "relative to the collision header -- see the fix there. Re-run the OOB check "
     "after regenerating local.json to confirm this drops out."),
    ("splitVertexIdx1", FieldStatus.CONFIRMED, "See splitVertexIdx0."),
    ("surfaceIdx0", FieldStatus.CONFIRMED,
     "Flat u8 index into surfaces; `state->point.field_C.cellSurfaces.surfaceIdx0 = "
     "subcell->surfaceIdx0` in func_8006B318. See splitVertexIdx0 re: earlier OOB result."),
    ("surfaceIdx1", FieldStatus.CONFIRMED, "See surfaceIdx0."),
]

# ---------------------------------------------------------------------------
# IPD_COLL_UNK3 -- 10 bytes.
# ---------------------------------------------------------------------------
IPD_COLL_UNK3_FORMAT = "<Hhhhh"
IPD_COLL_UNK3_FIELDS = [
    ("flags", FieldStatus.CONFIRMED, "e_GroundType + disableHeight + slope flags"),
    ("offsetX", FieldStatus.CONFIRMED, "offset X"),
    ("offsetY", FieldStatus.CONFIRMED, "offset Y"),
    ("offsetZ", FieldStatus.CONFIRMED, "offset Z"),
    ("field_8", FieldStatus.CONFIRMED, "q7_8"),
]

# ---------------------------------------------------------------------------
# IPD_COLL_UNK5 -- 1 byte.
# ---------------------------------------------------------------------------
IPD_COLL_UNK5_FORMAT = "<B"
IPD_COLL_UNK5_FIELDS = [("data", FieldStatus.CONFIRMED, None)]

# ---------------------------------------------------------------------------
# IPD_COLL_UNK6 -- 1 byte (same stride as block5/IPD_COLL_UNK5).
# Confirmed from collision.c func_8006CA18 which walks ptr_2C as `u8*`:
#   `for (curSurfaceIdx = &collData->ptr_2C[startIdx]; ...)`
# Each entry is a u8 index into collData->surfaces, NOT a 4-byte record.
# ---------------------------------------------------------------------------
IPD_COLL_UNK6_FORMAT = "<B"
IPD_COLL_UNK6_FIELDS = [("data", FieldStatus.CONFIRMED, None)]

# ---------------------------------------------------------------------------
# IPD_OBJNAME_DATA -- 16 bytes, confirmed by the "16 * dta.obj_id" indexing
# used in main.c.
# ---------------------------------------------------------------------------
IPD_OBJNAME_DATA_FORMAT = "<i8si"
IPD_OBJNAME_DATA_FIELDS = [
    ("flag", FieldStatus.CONFIRMED, "0 = mesh lives inside this IPD's embedded PLM; 1 = mesh lives in _GLB.PLM"),
    ("name", FieldStatus.CONFIRMED, "object name, matched against PLM_OBJ_HEADER.name"),
    ("unk", FieldStatus.RUNTIME_ONLY, "'here goes address when file is loaded by game' -- RAM pointer slot"),
]

# ---------------------------------------------------------------------------
# IPD_POS_HEADER -- 24 bytes, confirmed via THR0000.LOG arithmetic (see
# oracle/log_oracle.py): hpos(i) = obj_data_offset + 24*i reproduces every
# one of the 19 groups in the log exactly.
# ---------------------------------------------------------------------------
IPD_POS_HEADER_FORMAT = "<BBBBiiiii"
IPD_POS_HEADER_FIELDS = [
    ("obj_num", FieldStatus.CONFIRMED, "count of IPD_OBJ_DATA entries in this group"),
    ("unk1_num", FieldStatus.UNUSED,
     "'sub objects (tree leaves position data)?' -- only ever checked for the hardcoded name TREE02"),
    ("unk2_num", FieldStatus.UNUSED, "'drawing distance data length?'"),
    ("unk3_num", FieldStatus.UNUSED, "'always 0?'"),
    ("unk2", FieldStatus.UNUSED, "no hypothesis given in source at all"),
    ("unk3", FieldStatus.UNUSED, "no hypothesis given in source at all"),
    ("data_offset", FieldStatus.CONFIRMED, "offset to this group's IPD_OBJ_DATA array"),
    ("unk1_offset", FieldStatus.UNUSED,
     "used ONLY when unk1_num>0 AND object name == 'TREE02'; any other object with unk1_num>0 is silently skipped"),
    ("unk2_offset", FieldStatus.UNUSED, "'unknown2 data offset, 8 byte per pack?' -- never dereferenced"),
]

# ---------------------------------------------------------------------------
# IPD_OBJ_DATA -- 36 bytes, per placed instance (rotation matrix + translation).
# Confirmed stride via dpos deltas in THR0000.LOG (always exactly 0x24 apart
# within a group).
# ---------------------------------------------------------------------------
IPD_OBJ_DATA_FORMAT = "<i9hhiii"
IPD_OBJ_DATA_FIELDS = [
    ("obj_id", FieldStatus.CONFIRMED, "index into the IPD_OBJNAME_DATA table"),
    ("rt11", FieldStatus.CONFIRMED, "rotation matrix, row 1 (4096-scale fixed point)"),
    ("rt12", FieldStatus.CONFIRMED, None),
    ("rt13", FieldStatus.CONFIRMED, None),
    ("rt21", FieldStatus.CONFIRMED, "rotation matrix, row 2"),
    ("rt22", FieldStatus.CONFIRMED, None),
    ("rt23", FieldStatus.CONFIRMED, None),
    ("rt31", FieldStatus.CONFIRMED, "rotation matrix, row 3"),
    ("rt32", FieldStatus.CONFIRMED, None),
    ("rt33", FieldStatus.CONFIRMED, None),
    ("pad", FieldStatus.UNUSED, "alignment filler, forces tx onto a 4-byte boundary"),
    ("tx", FieldStatus.CONFIRMED, "translation; comment claims only 16 bits are meaningful -- UNVERIFIED"),
    ("ty", FieldStatus.CONFIRMED, None),
    ("tz", FieldStatus.CONFIRMED, None),
]

# ---------------------------------------------------------------------------
# PLM_FILE_HEADER -- 20 bytes.
# ---------------------------------------------------------------------------
PLM_FILE_HEADER_FORMAT = "<HBBiiii"
PLM_FILE_HEADER_FIELDS = [
    ("id", FieldStatus.CONFIRMED, "0x0630 for a valid PLM/ILM section"),
    ("flag", FieldStatus.UNUSED, "runtime-loaded flag, same category as IPD's flag field"),
    ("tex_num", FieldStatus.CONFIRMED, "count of 24-byte texture name entries"),
    ("tex_name_offset", FieldStatus.CONFIRMED, None),
    ("obj_num", FieldStatus.CONFIRMED, "count of PLM_OBJ_HEADER entries"),
    ("obj_start_offset", FieldStatus.CONFIRMED, None),
    ("unk_data_offset", FieldStatus.UNUSED, "'maybe object's index or drawing order' -- never dereferenced"),
]

# ---------------------------------------------------------------------------
# PLM_OBJ_HEADER -- 16 bytes.
# ---------------------------------------------------------------------------
PLM_OBJ_HEADER_FORMAT = "<8sBBBBi"
PLM_OBJ_HEADER_FIELDS = [
    ("name", FieldStatus.CONFIRMED, None),
    ("mesh_num", FieldStatus.CONFIRMED,
     "several THR0000 objects have mesh_num > 1 (THR0034, THR1701..THR1706, THR1703) -- confirmed from the log"),
    ("b", FieldStatus.UNUSED, "no hypothesis given in source"),
    ("c", FieldStatus.UNUSED, "no hypothesis given in source"),
    ("d", FieldStatus.UNUSED,
     "rendering flags per comment: bit0 shading, bits3-4 disable textures, bit5 semitransparency -- entirely dropped"),
    ("data_offset", FieldStatus.CONFIRMED, None),
]

# ---------------------------------------------------------------------------
# PLM_DATA_HEADER -- 24 bytes, one per mesh.
# ---------------------------------------------------------------------------
PLM_DATA_HEADER_FORMAT = "<BBBBiiiii"
PLM_DATA_HEADER_FIELDS = [
    ("pack_num", FieldStatus.CONFIRMED, "count of PLM_PACK_HEADER polygon packets"),
    ("vert_num", FieldStatus.CONFIRMED, "count of vertices"),
    ("num_c", FieldStatus.UNUSED, "'size = num_c * 4' of normal table per comment, but never used to bound anything"),
    ("num_d", FieldStatus.UNUSED, "'max index value' for normals per comment, never used to bound anything"),
    ("pack_offset", FieldStatus.CONFIRMED, None),
    ("vert_xy_offset", FieldStatus.CONFIRMED, None),
    ("vert_z_offset", FieldStatus.CONFIRMED, None),
    ("normal_offset", FieldStatus.CONFIRMED, "note: author's own comment on the normal-index logic is 'not sure if correct'"),
    ("end_offset", FieldStatus.UNUSED, "never used, not even as a sanity bound"),
]

# ---------------------------------------------------------------------------
# PLM_PACK_HEADER -- 20 bytes, one polygon packet (tri or quad).
# tex_num/unk2 are a single packed byte in C (bitfields tex_num:7, unk2:1).
# Python's struct module can't express C bitfields, so this is decoded by
# hand in code below. Bit order (tex_num in low 7 bits, unk2 in bit 7) is a
# HYPOTHESIS based on standard GCC/MSVC little-endian bitfield allocation
# order (first-declared field -> low bits); not yet independently confirmed
# against real data.
# ---------------------------------------------------------------------------
# Format: BB(u0,v0) H(cba) BB(u1,v1) BB(unk1,tex_unk2_byte) BB(u2,v2) BB(u3,v3) 4B(faces) 4B(normals)
# = 2+2+2+2+2+2+4+4 = 20 bytes, 19 scalar fields -- exactly matches C struct, no padding.
PLM_PACK_HEADER_FORMAT = "<BBHBBBBBBBBBBBBBBBB"
PLM_PACK_HEADER_FIELDS = [
    ("u0", FieldStatus.CONFIRMED, None),
    ("v0", FieldStatus.CONFIRMED, None),
    ("cba", FieldStatus.CONFIRMED, "CLUT position; fully consumed into baked pixel colors, not preserved as data"),
    ("u1", FieldStatus.CONFIRMED, None),
    ("v1", FieldStatus.CONFIRMED, None),
    ("unk1", FieldStatus.UNUSED, "appears only in a commented-out debug print -- never actually consumed"),
    ("tex_num_and_unk2_byte", FieldStatus.HYPOTHESIS, "packed byte: tex_num=low7 bits (CONFIRMED used), unk2=bit7 (CONFIRMED used, bit position is HYPOTHESIS)"),
    ("u2", FieldStatus.CONFIRMED, None),
    ("v2", FieldStatus.CONFIRMED, None),
    ("u3", FieldStatus.CONFIRMED, None),
    ("v3", FieldStatus.CONFIRMED, None),
    ("faces_0", FieldStatus.CONFIRMED, "face vertex index 0"),
    ("faces_1", FieldStatus.CONFIRMED, "face vertex index 1"),
    ("faces_2", FieldStatus.CONFIRMED, "face vertex index 2"),
    ("faces_3", FieldStatus.CONFIRMED, "face vertex index 3; 0xFF = triangle"),
    ("normals_0", FieldStatus.CONFIRMED, "normal index 0"),
    ("normals_1", FieldStatus.CONFIRMED, "normal index 1"),
    ("normals_2", FieldStatus.CONFIRMED, "normal index 2"),
    ("normals_3", FieldStatus.CONFIRMED, "normal index 3"),
]


# ---------------------------------------------------------------------------
# TIM_FILE_HEADER -- 8 bytes.
# ---------------------------------------------------------------------------
TIM_FILE_HEADER_FORMAT = "<BBBBBBBB"
TIM_FILE_HEADER_FIELDS = [
    ("id", FieldStatus.CONFIRMED, "0x10 for valid TIM"),
    ("ver", FieldStatus.UNUSED, "Version"),
    ("pad1_0", FieldStatus.UNUSED, None),
    ("pad1_1", FieldStatus.UNUSED, None),
    ("bpp_and_flags", FieldStatus.CONFIRMED, "bpp (2), pad_a (1), clp (1), pad_b (4)"),
    ("pad2_0", FieldStatus.UNUSED, None),
    ("pad2_1", FieldStatus.UNUSED, None),
    ("pad2_2", FieldStatus.UNUSED, None),
]

# ---------------------------------------------------------------------------
# TIM_CLUT_HEADER -- 12 bytes.
# ---------------------------------------------------------------------------
TIM_CLUT_HEADER_FORMAT = "<iHHHH"
TIM_CLUT_HEADER_FIELDS = [
    ("clut_length", FieldStatus.CONFIRMED, "Total size of CLUT block in bytes (including this header)"),
    ("x", FieldStatus.CONFIRMED, "CLUT X coordinate in VRAM"),
    ("y", FieldStatus.CONFIRMED, "CLUT Y coordinate in VRAM"),
    ("width", FieldStatus.CONFIRMED, "CLUT width"),
    ("height", FieldStatus.CONFIRMED, "CLUT height"),
]

# ---------------------------------------------------------------------------
# TIM_IMG_HEADER -- 12 bytes.
# ---------------------------------------------------------------------------
TIM_IMG_HEADER_FORMAT = "<iHHHH"
TIM_IMG_HEADER_FIELDS = [
    ("img_length", FieldStatus.CONFIRMED, "Total size of Image block in bytes (including this header)"),
    ("x", FieldStatus.CONFIRMED, "Image X coordinate in VRAM"),
    ("y", FieldStatus.CONFIRMED, "Image Y coordinate in VRAM"),
    ("width", FieldStatus.CONFIRMED, "Image width"),
    ("height", FieldStatus.CONFIRMED, "Image height"),
]

# ---------------------------------------------------------------------------
# TGA_FILE_HEADER -- 18 bytes. Packed strictly, 1-byte aligned.
# ---------------------------------------------------------------------------
TGA_FILE_HEADER_FORMAT = "<BBBHHBHHHHBB"
TGA_FILE_HEADER_FIELDS = [
    ("id_len", FieldStatus.CONFIRMED, "id length"),
    ("color_map_type", FieldStatus.CONFIRMED, "colour map type"),
    ("image_type", FieldStatus.CONFIRMED, "image type"),
    ("cm_first_entry", FieldStatus.CONFIRMED, "colour map first entry"),
    ("cm_length", FieldStatus.CONFIRMED, "colour map length"),
    ("map_entry_size", FieldStatus.CONFIRMED, "map entry size (16, 24, 32)"),
    ("h_origin", FieldStatus.CONFIRMED, "horizontal origin"),
    ("v_origin", FieldStatus.CONFIRMED, "vertical origin"),
    ("width", FieldStatus.CONFIRMED, "width"),
    ("height", FieldStatus.CONFIRMED, "height"),
    ("pixel_depth", FieldStatus.CONFIRMED, "pixel depth"),
    ("image_desc", FieldStatus.CONFIRMED, "image descriptor"),
]


def _verify_sizes() -> None:
    """Cross-check every format string's calcsize against the hand-derived
    total from the field list above. Raises AssertionError on mismatch --
    this is meant to fail loudly the moment any struct definition drifts
    from what was actually confirmed in main.c."""
    checks = [
        ("IPD_FILE_HEADER", IPD_FILE_HEADER_FORMAT, 84),
        ("IPD_COLL_HEADER", IPD_COLL_HEADER_FORMAT, 308),
        ("IPD_COLL_SVECTOR", IPD_COLL_SVECTOR_FORMAT, 6),
        ("IPD_COLL_SURFACE", IPD_COLL_SURFACE_FORMAT, 12),
        ("IPD_COLL_SUBCELL", IPD_COLL_SUBCELL_FORMAT, 10),
        ("IPD_COLL_UNK3", IPD_COLL_UNK3_FORMAT, 10),
        ("IPD_COLL_UNK5", IPD_COLL_UNK5_FORMAT, 1),
        ("IPD_COLL_UNK6", IPD_COLL_UNK6_FORMAT, 1),
        ("IPD_OBJNAME_DATA", IPD_OBJNAME_DATA_FORMAT, 16),
        ("IPD_POS_HEADER", IPD_POS_HEADER_FORMAT, 24),
        ("IPD_OBJ_DATA", IPD_OBJ_DATA_FORMAT, 36),
        ("PLM_FILE_HEADER", PLM_FILE_HEADER_FORMAT, 20),
        ("PLM_OBJ_HEADER", PLM_OBJ_HEADER_FORMAT, 16),
        ("PLM_DATA_HEADER", PLM_DATA_HEADER_FORMAT, 24),
        ("PLM_PACK_HEADER", PLM_PACK_HEADER_FORMAT, 20),
        ("TIM_FILE_HEADER", TIM_FILE_HEADER_FORMAT, 8),
        ("TIM_CLUT_HEADER", TIM_CLUT_HEADER_FORMAT, 12),
        ("TIM_IMG_HEADER", TIM_IMG_HEADER_FORMAT, 12),
        ("TGA_FILE_HEADER", TGA_FILE_HEADER_FORMAT, 18),
    ]
    for name, fmt, expected in checks:
        actual = struct.calcsize(fmt)
        status = "OK" if actual == expected else "MISMATCH"
        print(f"{name:20s} fmt={fmt:22s} calcsize={actual:3d}  expected={expected:3d}  [{status}]")
        assert actual == expected, f"{name}: calcsize {actual} != expected {expected}"


if __name__ == "__main__":
    _verify_sizes()
    print("\nAll struct sizes verified against hand-derived totals.")


"""
manifest.py -- byte-exact structured records for the SH1 IPD/PLM formats.

WHY THIS EXISTS: a printf-style verbose log is a fine human-readable
companion, but it's a poor *machine* intermediate for "edit a value, then
re-emit an exact IPD" -- you'd need a second parser just for your own log
format, with all the same precision-loss traps (sign extension,
hex-vs-decimal ambiguity, whitespace fragility) we already hit with
ipd2obj's `%f` floats. This module is the alternative: every struct we've
confirmed becomes a small dataclass that can decode itself from raw bytes
and re-encode itself back to IDENTICAL bytes. That symmetry
(decode -> re-encode -> compare) is both the correctness test for this
module AND, later, most of the actual IPD-writer logic.

Anything we have NOT confirmed (IPD_COL_HEADER, and any other byte range
no confirmed struct claims) is deliberately *not* modeled as a decoded
record here -- it stays as an opaque raw-bytes blob (see UnknownRegion
below) that gets copied verbatim. We do not need to understand a byte
range to preserve it exactly; conflating "understood" with "preserved"
is exactly the mistake that would make this fragile.

Every Record class below reuses the format strings from ipd_structs.py
verbatim (imported, not retyped) so there is exactly one place the byte
layout is ever written down.
"""

import struct
from dataclasses import dataclass
from typing import ClassVar



class Record:
    """Base class giving every dataclass subclass byte-exact from_bytes/
    to_bytes, derived mechanically from a struct.Struct + a field-name
    tuple (in on-disk order). Subclasses set _struct and _field_order."""

    _struct: ClassVar[struct.Struct]
    _field_order: ClassVar[tuple[str, ...]]

    @classmethod
    def size(cls) -> int:
        return cls._struct.size

    @classmethod
    def from_bytes(cls, buf: bytes, offset: int = 0) -> "Record":
        raw = buf[offset:offset + cls._struct.size]
        if len(raw) != cls._struct.size:
            raise ValueError(
                f"{cls.__name__}.from_bytes: need {cls._struct.size} bytes at offset {offset}, "
                f"only {len(raw)} available (truncated file?)"
            )
        values = cls._struct.unpack(raw)
        return cls(*values)

    def to_bytes(self) -> bytes:
        values = tuple(getattr(self, name) for name in self._field_order)
        return self._struct.pack(*values)

    def roundtrip_ok(self, original: bytes) -> bool:
        """The actual correctness test for a record class: decode(x) then
        re-encode must reproduce x exactly. Call this obsessively once we
        have real bytes -- any False here means our struct layout, not
        just our understanding, is wrong."""
        return self.to_bytes() == original


def _make(name: str, fmt: str, field_names: list[str]):
    """Build a Record subclass from a verified format string + field
    names, in on-disk order. Raises if the field-name count doesn't match
    what the format string actually unpacks to -- fails loudly rather
    than silently misaligning fields."""
    compiled = struct.Struct(fmt)
    probe = compiled.unpack(b"\x00" * compiled.size)
    if len(probe) != len(field_names):
        raise ValueError(
            f"{name}: format {fmt!r} unpacks to {len(probe)} values "
            f"but {len(field_names)} field names were given -- these must match 1:1"
        )
    # Dynamically build a dataclass with the given field names
    annotations = {n: object for n in field_names}
    cls = dataclass(type(name, (Record,), {"__annotations__": annotations}))
    cls._struct = compiled
    cls._field_order = tuple(field_names)
    return cls


# ---------------------------------------------------------------------------
# One Record subclass per CONFIRMED struct. Field order matches
# ipd_structs.py's *_FIELDS lists exactly (which is itself the on-disk
# order, cross-checked against main.c).
# ---------------------------------------------------------------------------

IPDFileHeader = _make(
    "IPDFileHeader", IPD_FILE_HEADER_FORMAT,
    [n for n, _, _ in IPD_FILE_HEADER_FIELDS],
)
IPDCollHeader = _make(
    "IPDCollHeader", IPD_COLL_HEADER_FORMAT,
    [n for n, _, _ in IPD_COLL_HEADER_FIELDS],
)
IPDCollSVector = _make(
    "IPDCollSVector", IPD_COLL_SVECTOR_FORMAT,
    [n for n, _, _ in IPD_COLL_SVECTOR_FIELDS],
)
IPDCollSurface = _make(
    "IPDCollSurface", IPD_COLL_SURFACE_FORMAT,
    [n for n, _, _ in IPD_COLL_SURFACE_FIELDS],
)
IPDCollSubcell = _make(
    "IPDCollSubcell", IPD_COLL_SUBCELL_FORMAT,
    [n for n, _, _ in IPD_COLL_SUBCELL_FIELDS],
)
IPDCollUnk3 = _make(
    "IPDCollUnk3", IPD_COLL_UNK3_FORMAT,
    [n for n, _, _ in IPD_COLL_UNK3_FIELDS],
)
IPDCollUnk5 = _make(
    "IPDCollUnk5", IPD_COLL_UNK5_FORMAT,
    [n for n, _, _ in IPD_COLL_UNK5_FIELDS],
)
IPDCollUnk6 = _make(
    "IPDCollUnk6", IPD_COLL_UNK6_FORMAT,
    [n for n, _, _ in IPD_COLL_UNK6_FIELDS],
)
IPDObjNameData = _make(
    "IPDObjNameData", IPD_OBJNAME_DATA_FORMAT,
    [n for n, _, _ in IPD_OBJNAME_DATA_FIELDS],
)
IPDPosHeader = _make(
    "IPDPosHeader", IPD_POS_HEADER_FORMAT,
    [n for n, _, _ in IPD_POS_HEADER_FIELDS],
)
IPDObjData = _make(
    "IPDObjData", IPD_OBJ_DATA_FORMAT,
    [n for n, _, _ in IPD_OBJ_DATA_FIELDS],
)
PLMFileHeader = _make(
    "PLMFileHeader", PLM_FILE_HEADER_FORMAT,
    [n for n, _, _ in PLM_FILE_HEADER_FIELDS],
)
PLMObjHeader = _make(
    "PLMObjHeader", PLM_OBJ_HEADER_FORMAT,
    [n for n, _, _ in PLM_OBJ_HEADER_FIELDS],
)
PLMDataHeader = _make(
    "PLMDataHeader", PLM_DATA_HEADER_FORMAT,
    [n for n, _, _ in PLM_DATA_HEADER_FIELDS],
)
# NOTE: PLM_PACK_HEADER's tex_num/unk2 packed byte is deliberately left as
# one raw field here (tex_num_and_unk2_byte) rather than pre-split, because
# the bit order is HYPOTHESIS until confirmed against real data.
# The faces/normals are stored as individual scalar fields (faces_0..faces_3,
# normals_0..normals_3) matching the struct unpack shape exactly.
PLMPackHeader = _make(
    "PLMPackHeader", PLM_PACK_HEADER_FORMAT,
    [n for n, _, _ in PLM_PACK_HEADER_FIELDS],
)
TIMFileHeader = _make(
    "TIMFileHeader", TIM_FILE_HEADER_FORMAT,
    [n for n, _, _ in TIM_FILE_HEADER_FIELDS],
)
TIMClutHeader = _make(
    "TIMClutHeader", TIM_CLUT_HEADER_FORMAT,
    [n for n, _, _ in TIM_CLUT_HEADER_FIELDS],
)
TIMImgHeader = _make(
    "TIMImgHeader", TIM_IMG_HEADER_FORMAT,
    [n for n, _, _ in TIM_IMG_HEADER_FIELDS],
)
TGAFileHeader = _make(
    "TGAFileHeader", TGA_FILE_HEADER_FORMAT,
    [n for n, _, _ in TGA_FILE_HEADER_FIELDS],
)


# Convenience properties on PLMPackHeader
def _plm_pack_faces(self) -> tuple[int, int, int, int]:
    return (self.faces_0, self.faces_1, self.faces_2, self.faces_3)


def _plm_pack_normals(self) -> tuple[int, int, int, int]:
    return (self.normals_0, self.normals_1, self.normals_2, self.normals_3)


def _plm_pack_tex_num(self) -> int:
    """Low 7 bits of the packed byte -- HYPOTHESIS on bit order."""
    return self.tex_num_and_unk2_byte & 0x7F


def _plm_pack_unk2(self) -> int:
    """Bit 7 of the packed byte -- same HYPOTHESIS status."""
    return (self.tex_num_and_unk2_byte >> 7) & 0x1


PLMPackHeader.faces = property(_plm_pack_faces)
PLMPackHeader.normals = property(_plm_pack_normals)
PLMPackHeader.tex_num = property(_plm_pack_tex_num)
PLMPackHeader.unk2 = property(_plm_pack_unk2)


@dataclass
class UnknownRegion:
    """An opaque, byte-exact span of the file with no confirmed struct.
    We never try to interpret this -- only to locate it precisely and
    copy it verbatim. label is human-context only, never used for encoding."""
    start: int
    raw: bytes
    label: str = ""

    @property
    def end(self) -> int:
        return self.start + len(self.raw)


def compute_gaps(file_size: int, covered: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Given a list of (start, end) byte ranges already claimed by decoded
    records, return the complementary list of (start, end) ranges NOT
    covered by anything -- the empirical, no-assumptions way to find
    'what did we miss', independent of any hypothesis about what should
    be there."""
    if not covered:
        return [(0, file_size)]
    ranges = sorted(covered)
    gaps = []
    cursor = 0
    for start, end in ranges:
        if start > cursor:
            gaps.append((cursor, start))
        elif start < cursor:
            raise ValueError(
                f"Overlapping covered ranges detected: cursor={cursor}, next=({start},{end}) "
                f"-- two records claim the same bytes, our model is inconsistent"
            )
        cursor = max(cursor, end)
    if cursor < file_size:
        gaps.append((cursor, file_size))
    return gaps


# ---------------------------------------------------------------------------
# Self-test: prove the (de)serialization machinery itself is correct using
# SYNTHETIC data, before we ever touch a real file. This is deliberately
# independent of any real IPD bytes -- it only tests "does our own
# encode/decode round-trip agree with itself".
# ---------------------------------------------------------------------------

def _self_test() -> None:
    import random
    rng = random.Random(1234)  # deterministic, reproducible

    def random_bytes(n):
        return bytes(rng.randrange(256) for _ in range(n))

    all_record_classes = [
        IPDFileHeader, IPDCollHeader, IPDCollSVector, IPDCollSurface, IPDCollSubcell,
        IPDCollUnk3, IPDCollUnk5, IPDCollUnk6,
        IPDObjNameData, IPDPosHeader, IPDObjData,
        PLMFileHeader, PLMObjHeader, PLMDataHeader, PLMPackHeader,
        TIMFileHeader, TIMClutHeader, TIMImgHeader, TGAFileHeader,
    ]

    for cls in all_record_classes:
        # Fuzz with 50 random byte buffers of exactly the right size --
        # decode, re-encode, and demand byte-for-byte equality.
        for trial in range(50):
            raw = random_bytes(cls.size())
            rec = cls.from_bytes(raw)
            re_encoded = rec.to_bytes()
            assert re_encoded == raw, (
                f"{cls.__name__} trial {trial}: round trip FAILED\n"
                f"  original   = {raw.hex()}\n"
                f"  re-encoded = {re_encoded.hex()}"
            )
        print(f"{cls.__name__:20s} size={cls.size():3d}  50/50 random round-trips OK")

    # compute_gaps sanity checks
    assert compute_gaps(100, []) == [(0, 100)]
    assert compute_gaps(100, [(0, 100)]) == []
    assert compute_gaps(100, [(10, 20), (50, 60)]) == [(0, 10), (20, 50), (60, 100)]
    assert compute_gaps(100, [(0, 10)]) == [(10, 100)]
    try:
        compute_gaps(100, [(0, 20), (10, 30)])
        raise AssertionError("compute_gaps should have raised on overlapping ranges")
    except ValueError:
        pass
    print("compute_gaps          all sanity checks OK")

    # Concrete "edit one field, only those bytes change" test
    values = list(range(len(IPDObjData._field_order)))
    original = IPDObjData(*values)
    original_bytes = original.to_bytes()

    edited = IPDObjData(**{**original.__dict__, "tx": 99999})
    edited_bytes = edited.to_bytes()

    diff_positions = [i for i in range(len(original_bytes)) if original_bytes[i] != edited_bytes[i]]
    prefix_struct = struct.Struct("<i9hh")  # everything before tx
    expected_tx_start = prefix_struct.size
    print(f"IPDObjData edit test: changed only 'tx', bytes that differ = {diff_positions} "
          f"(expected to start at offset {expected_tx_start})")
    assert diff_positions and min(diff_positions) == expected_tx_start, \
        "editing tx touched bytes outside tx's own field -- something is wrong with field order/packing"

    print("\nAll manifest self-tests passed (synthetic data only -- still need real IPD bytes for the real test).")


if __name__ == "__main__":
    _self_test()
