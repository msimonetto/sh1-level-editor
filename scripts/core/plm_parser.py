from __future__ import annotations
import struct
import sys
from typing import Optional
from .models import PLMFileHeader, PLMObjHeader, PLMDataHeader, PLMPackHeader, TIMFileHeader, TIMClutHeader, TIMImgHeader
from .utils import _hex_blob, _record_to_dict, _merge_ranges, _gaps_from_merged, _write_at
from . import models as S

def serialise_plm(buf: bytes, plm_base: int, source_label: str) -> dict:
    """
    Fully serialise one PLM section (embedded in IPD or standalone GLB).

    buf        -- the full source file buffer (IPD or standalone PLM)
    plm_base   -- absolute byte offset where the PLM section starts in buf
    source_label -- e.g. "THR0000.IPD" or "THR_GLB.PLM"
    """
    plm_hdr = PLMFileHeader.from_bytes(buf, plm_base)
    if plm_hdr.id != 0x0630:
        raise ValueError(
            f"PLM magic 0x{plm_hdr.id:04X} at offset 0x{plm_base:04X}; expected 0x0630"
        )

    result: dict = {
        "source_file": source_label,
        "base_offset": plm_base,
        "plm_header": _record_to_dict(plm_hdr, S.PLM_FILE_HEADER_FIELDS),
    }

    # --- Texture name table (24-byte null-padded entries)
    abs_tex = plm_base + plm_hdr.tex_name_offset
    tex_names = []
    tex_names_raw_hex = []
    for i in range(plm_hdr.tex_num):
        raw = buf[abs_tex + i * 24: abs_tex + (i + 1) * 24]
        name = raw.rstrip(b"\x00").decode("ascii", errors="replace")
        tex_names.append(name)
        tex_names_raw_hex.append(raw.hex())
    result["tex_names"] = tex_names
    result["tex_names_raw_hex"] = tex_names_raw_hex

    # --- Object headers + geometry
    abs_obj = plm_base + plm_hdr.obj_start_offset
    covered = [
        (plm_base, plm_base + PLMFileHeader.size()),
        (abs_tex, abs_tex + plm_hdr.tex_num * 24),
        (abs_obj, abs_obj + plm_hdr.obj_num * PLMObjHeader.size()),
    ]

    obj_headers_json = []
    for i in range(plm_hdr.obj_num):
        obj_off = abs_obj + i * PLMObjHeader.size()
        obj = PLMObjHeader.from_bytes(buf, obj_off)
        obj_name = obj.name.rstrip(b"\x00").decode("ascii", errors="replace")
        abs_data = plm_base + obj.data_offset

        obj_json: dict = {
            "name": obj_name,
            "name_raw_hex": obj.name.hex(),
            "mesh_num": obj.mesh_num,
            "b": obj.b,
            "c": obj.c,
            "d": obj.d,
            "data_offset": obj.data_offset,
        }

        meshes_json = []
        for m in range(obj.mesh_num):
            dh_off = abs_data + m * PLMDataHeader.size()
            dh = PLMDataHeader.from_bytes(buf, dh_off)

            abs_pack = plm_base + dh.pack_offset
            abs_vxy  = plm_base + dh.vert_xy_offset
            abs_vz   = plm_base + dh.vert_z_offset
            abs_norm = plm_base + dh.normal_offset

            # Track covered regions
            covered.append((dh_off, dh_off + PLMDataHeader.size()))
            if dh.pack_num > 0:
                covered.append((abs_pack, abs_pack + dh.pack_num * PLMPackHeader.size()))
            if dh.vert_num > 0:
                covered.append((abs_vxy, abs_vxy + dh.vert_num * 4))
                covered.append((abs_vz,  abs_vz  + dh.vert_num * 2))
            # IMPORTANT: num_d is the actual count of normal entries (max index + 1),
            # not num_c (which is the "logical" count used for bounds in comments).
            # ipd_to_obj.py reads up to n_idx*4 for any n_idx in the packs, and
            # n_idx can be as large as num_d-1. Use num_d for coverage.
            if dh.num_d > 0:
                covered.append((abs_norm, abs_norm + dh.num_d * 4))

            # --- Vertices XY
            verts_xy = []
            for k in range(dh.vert_num):
                x, y = struct.unpack_from("<hh", buf, abs_vxy + k * 4)
                verts_xy.append([x, y])

            # --- Vertices Z
            verts_z = []
            for k in range(dh.vert_num):
                z, = struct.unpack_from("<h", buf, abs_vz + k * 2)
                verts_z.append(z)

            # --- Normals (num_d entries × 4 bytes, last byte padding)
            # IMPORTANT: store num_d entries (not num_c). Pack normal indices can
            # reference entries 0..num_d-1. num_c appears to be only the first logical
            # cluster; packs freely index beyond it. Read as unsigned bytes (<BBBB)
            # to match ipd_to_obj.py's '<BBB' reads exactly.
            # BOUNDS CHECK: some IPDs have num_d * 4 running past EOF (as in SU0001).
            # Match ipd_to_obj.py's condition: only read entry n if n_off + 3 <= len(buf).
            normals = []
            for n in range(dh.num_d):
                n_off = abs_norm + n * 4
                if n_off + 3 <= len(buf):
                    nx, ny, nz, _pad = struct.unpack_from("<BBBB", buf, n_off)
                    normals.append([nx, ny, nz, _pad])
                else:
                    break  # stop: remaining entries exceed file bounds


            # --- Polygon packs
            packs_json = []
            for p in range(dh.pack_num):
                pk = PLMPackHeader.from_bytes(buf, abs_pack + p * PLMPackHeader.size())
                packs_json.append({
                    "u0": pk.u0, "v0": pk.v0,
                    "cba": pk.cba,
                    "u1": pk.u1, "v1": pk.v1,
                    "unk1": pk.unk1,
                    "tex_num_and_unk2_byte": pk.tex_num_and_unk2_byte,
                    "u2": pk.u2, "v2": pk.v2,
                    "u3": pk.u3, "v3": pk.v3,
                    "faces_0": pk.faces_0, "faces_1": pk.faces_1,
                    "faces_2": pk.faces_2, "faces_3": pk.faces_3,
                    "normals_0": pk.normals_0, "normals_1": pk.normals_1,
                    "normals_2": pk.normals_2, "normals_3": pk.normals_3,
                })

            meshes_json.append({
                "data_header": _record_to_dict(dh, S.PLM_DATA_HEADER_FIELDS),
                "vertices_xy": verts_xy,
                "vertices_z":  verts_z,
                "normals":     normals,
                "packs":       packs_json,
            })

        obj_json["meshes"] = meshes_json
        obj_headers_json.append(obj_json)

    result["obj_headers"] = obj_headers_json

    # --- Gap analysis within the PLM section
    # Use a merge-tolerant variant: sort and merge overlapping covered ranges first,
    # then compute gaps. This is necessary because num_d normal ranges can overlap
    # with packs/vertices of adjacent meshes (the binary regions interleave).
    plm_end = len(buf)
    merged = _merge_ranges(covered)
    all_gaps = _gaps_from_merged(plm_end, merged)

    # Only report gaps that fall within the PLM section
    plm_gaps = []
    for gap_start, gap_end in all_gaps:
        if gap_start >= plm_base:
            plm_gaps.append({
                "start": gap_start,
                "end": gap_end,
                "label": "unknown",
                "hex": _hex_blob(buf, gap_start, gap_end),
            })

    result["gaps"] = plm_gaps
    return result


# ---------------------------------------------------------------------------
# IPD top-level serialiser
# ---------------------------------------------------------------------------

def _encode_plm(plm_json: dict, original_plm_base: int, source_size: int | None = None) -> bytes:
    """
    Re-encode a PLM section from its JSON representation, placing every
    sub-region at exactly its original file offset relative to the PLM base.

    plm_json          -- dict produced by serialise_plm()
    original_plm_base -- original absolute byte offset of the PLM section.
                         Used to convert absolute gap offsets to relative.

    Returns raw bytes for the complete PLM section.
    """
    plm_hdr_d   = plm_json["plm_header"]
    obj_headers = plm_json["obj_headers"]
    tex_num     = len(plm_json["tex_names_raw_hex"])
    obj_num     = len(obj_headers)

    max_end_rel = 0   # relative to PLM base

    PLM_HDR_SIZE  = PLMFileHeader.size()   # 20
    TEX_ENTRY_SZ  = 24
    OBJ_HDR_SIZE  = PLMObjHeader.size()   # 16
    DATA_HDR_SIZE = PLMDataHeader.size()  # 24
    PACK_SIZE     = PLMPackHeader.size()  # 20

    def _update_max(rel_start: int, size: int) -> None:
        nonlocal max_end_rel
        end = rel_start + size
        if end > max_end_rel:
            max_end_rel = end

    # PLM header (offset 0 relative to PLM base)
    _update_max(0, PLM_HDR_SIZE)
    # Tex names
    _update_max(plm_hdr_d["tex_name_offset"], tex_num * TEX_ENTRY_SZ)
    # Obj headers
    _update_max(plm_hdr_d["obj_start_offset"], obj_num * OBJ_HDR_SIZE)

    for obj_json in obj_headers:
        data_offset_rel = obj_json["data_offset"]   # rel to PLM base
        for mi, mesh in enumerate(obj_json["meshes"]):
            mh = mesh["data_header"]
            dh_rel = data_offset_rel + mi * DATA_HDR_SIZE
            _update_max(dh_rel, DATA_HDR_SIZE)
            _update_max(mh["pack_offset"],    mh["pack_num"] * PACK_SIZE)
            _update_max(mh["vert_xy_offset"], mh["vert_num"] * 4)
            _update_max(mh["vert_z_offset"],  mh["vert_num"] * 2)
            _update_max(mh["normal_offset"],  mh["num_d"] * 4)

    # Gaps: stored as absolute file offsets; convert to PLM-relative
    for gap in plm_json.get("gaps", []):
        gap_start_abs = gap["start"]
        gap_end_abs   = gap["end"]
        gap_start_rel = gap_start_abs - original_plm_base
        _update_max(gap_start_rel, gap_end_abs - gap_start_abs)

    if source_size is not None:
        plm_size = source_size - original_plm_base
        if max_end_rel > plm_size:
            max_end_rel = plm_size

    plm_buf = bytearray(max_end_rel)

    # ----------------------------------------------------------------
    # Write PLM_FILE_HEADER at offset 0
    # ----------------------------------------------------------------
    plm_hdr = PLMFileHeader(
        id               = plm_hdr_d["id"],
        flag             = plm_hdr_d["flag"],
        tex_num          = tex_num,
        tex_name_offset  = plm_hdr_d["tex_name_offset"],
        obj_num          = obj_num,
        obj_start_offset = plm_hdr_d["obj_start_offset"],
        unk_data_offset  = plm_hdr_d["unk_data_offset"],
    )
    _write_at(plm_buf, 0, plm_hdr.to_bytes())

    # ----------------------------------------------------------------
    # Write texture name entries
    # ----------------------------------------------------------------
    abs_tex_rel = plm_hdr_d["tex_name_offset"]
    for i, raw_hex in enumerate(plm_json["tex_names_raw_hex"]):
        raw = bytes.fromhex(raw_hex)
        assert len(raw) == TEX_ENTRY_SZ
        _write_at(plm_buf, abs_tex_rel + i * TEX_ENTRY_SZ, raw)

    # ----------------------------------------------------------------
    # Write object headers and geometry
    # ----------------------------------------------------------------
    abs_obj_rel = plm_hdr_d["obj_start_offset"]
    for i, obj_json in enumerate(obj_headers):
        name_raw = bytes.fromhex(obj_json["name_raw_hex"])
        oh = PLMObjHeader(
            name        = name_raw,
            mesh_num    = obj_json["mesh_num"],
            b           = obj_json["b"],
            c           = obj_json["c"],
            d           = obj_json["d"],
            data_offset = obj_json["data_offset"],  # original relative offset
        )
        _write_at(plm_buf, abs_obj_rel + i * OBJ_HDR_SIZE, oh.to_bytes())

        data_offset_rel = obj_json["data_offset"]   # rel to PLM base
        for mi, mesh in enumerate(obj_json["meshes"]):
            mh_d = mesh["data_header"]

            # --- Write PLMDataHeader at original position
            dh_rel = data_offset_rel + mi * DATA_HDR_SIZE
            dh = PLMDataHeader(
                pack_num       = mh_d["pack_num"],
                vert_num       = mh_d["vert_num"],
                num_c          = mh_d["num_c"],
                num_d          = mh_d["num_d"],
                pack_offset    = mh_d["pack_offset"],
                vert_xy_offset = mh_d["vert_xy_offset"],
                vert_z_offset  = mh_d["vert_z_offset"],
                normal_offset  = mh_d["normal_offset"],
                end_offset     = mh_d["end_offset"],
            )
            _write_at(plm_buf, dh_rel, dh.to_bytes())

            # --- Packs at pack_offset (relative to PLM base)
            pack_rel = mh_d["pack_offset"]
            for pi, pk_d in enumerate(mesh["packs"]):
                pk = PLMPackHeader(
                    u0  = pk_d["u0"],  v0  = pk_d["v0"],
                    cba = pk_d["cba"],
                    u1  = pk_d["u1"],  v1  = pk_d["v1"],
                    unk1 = pk_d["unk1"],
                    tex_num_and_unk2_byte = pk_d["tex_num_and_unk2_byte"],
                    u2  = pk_d["u2"],  v2  = pk_d["v2"],
                    u3  = pk_d["u3"],  v3  = pk_d["v3"],
                    faces_0   = pk_d["faces_0"],   faces_1   = pk_d["faces_1"],
                    faces_2   = pk_d["faces_2"],   faces_3   = pk_d["faces_3"],
                    normals_0 = pk_d["normals_0"], normals_1 = pk_d["normals_1"],
                    normals_2 = pk_d["normals_2"], normals_3 = pk_d["normals_3"],
                )
                _write_at(plm_buf, pack_rel + pi * PACK_SIZE, pk.to_bytes())

            # --- Vertices XY at vert_xy_offset (relative to PLM base)
            vxy_rel = mh_d["vert_xy_offset"]
            for vi, (x, y) in enumerate(mesh["vertices_xy"]):
                _write_at(plm_buf, vxy_rel + vi * 4, struct.pack("<hh", x, y))

            # --- Vertices Z at vert_z_offset (relative to PLM base)
            vz_rel = mh_d["vert_z_offset"]
            for vi, z in enumerate(mesh["vertices_z"]):
                _write_at(plm_buf, vz_rel + vi * 2, struct.pack("<h", z))

            # --- Normals at normal_offset (relative to PLM base)
            norm_rel = mh_d["normal_offset"]
            for ni, n in enumerate(mesh["normals"]):
                if len(n) == 4:
                    nx, ny, nz, _pad = n
                else:
                    nx, ny, nz = n[:3]
                    _pad = 0
                _write_at(plm_buf, norm_rel + ni * 4, struct.pack("<BBBB", nx, ny, nz, _pad))

    # ----------------------------------------------------------------
    # Write gap blobs verbatim at their original positions
    # ----------------------------------------------------------------
    for gap in plm_json.get("gaps", []):
        gap_start_abs = gap["start"]
        gap_start_rel = gap_start_abs - original_plm_base
        gap_data = bytes.fromhex(gap["hex"])
        _write_at(plm_buf, gap_start_rel, gap_data)

    return bytes(plm_buf)


# ---------------------------------------------------------------------------
# IPD top-level decoder
# ---------------------------------------------------------------------------

