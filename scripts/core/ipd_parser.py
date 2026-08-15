from __future__ import annotations
import json
import struct
import sys
from typing import Optional
from pathlib import Path
from .models import IPDFileHeader, IPDCollHeader, IPDCollSVector, IPDCollSurface, IPDCollSubcell, IPDCollUnk3, IPDCollUnk5, IPDCollUnk6, IPDObjNameData, IPDPosHeader, IPDObjData, compute_gaps, IPD_FILE_HEADER_FIELDS, IPD_COLL_HEADER_FIELDS
from .utils import _hex_blob, _record_to_dict, _dict_to_record, _write_at, _merge_ranges
from . import models as S
from .plm_parser import _encode_plm, serialise_plm

def serialise_local_ipd(ipd_path: Path, map_assets: dict | None = None) -> dict:
    """Read an IPD file and return a lean local JSON-serialisable dict."""
    buf = ipd_path.read_bytes()
    ipd_size = len(buf)
    prefix = ipd_path.stem.rstrip('0123456789ABCD') # e.g. "THR0000" -> "THR"

    ipd_hdr = IPDFileHeader.from_bytes(buf, 0)
    print(f"\nSerialising Local IPD: {ipd_path.name}  ({ipd_size} bytes)")

    result: dict = {
        "source_file": ipd_path.name,
        "source_size": ipd_size,
        "ipd_header": _record_to_dict(ipd_hdr, S.IPD_FILE_HEADER_FIELDS),
    }

    header_end = IPDFileHeader.size()
    obj_name_start = ipd_hdr.obj_name_offset
    if obj_name_start > header_end:
        gap_size = obj_name_start - header_end
        if gap_size == 308:
            coll_hdr = IPDCollHeader.from_bytes(buf, header_end)
            result["ipd_collision_header"] = _record_to_dict(coll_hdr, S.IPD_COLL_HEADER_FIELDS)
            # subcellCheckIdx is huge and can be confusing in JSON, let's keep it as hex
            if "subcellCheckIdx" in result["ipd_collision_header"] and isinstance(result["ipd_collision_header"]["subcellCheckIdx"], bytes):
                result["ipd_collision_header"]["subcellCheckIdx"] = result["ipd_collision_header"]["subcellCheckIdx"].hex()

            # Ipd_CollisionPtrsInit (collision.c, 0x8006993C) patches these fields at
            # runtime as `raw + (u32)collData` -- i.e. every ptr_* field here is stored
            # on-disk as an offset *relative to the start of this 308-byte collision
            # header* (== header_end), NOT as an absolute file offset from byte 0.
            # Reproduce that patch here so every downstream reader can treat
            # hdr["ptr_*"] as a plain absolute file offset. ptr_unk7 is deliberately
            # excluded -- Ipd_CollisionPtrsInit never touches it.
            for _ptr_field in ("ptr_splitVertices", "ptr_surfaces", "ptr_subcells",
                                "ptr_unkBlock3", "ptr_grid", "ptr_block5", "ptr_block6"):
                result["ipd_collision_header"][_ptr_field] += header_end
        else:
            result["gap_header_to_obj_name"] = _hex_blob(buf, header_end, obj_name_start)
    else:
        result["gap_header_to_obj_name"] = ""

    name_entries = []
    for i in range(ipd_hdr.obj_num):
        off = ipd_hdr.obj_name_offset + i * IPDObjNameData.size()
        nd = IPDObjNameData.from_bytes(buf, off)
        name_str = nd.name.rstrip(b"\x00").decode("ascii", errors="replace")
        
        entry = {
            "flag": nd.flag,
            "name": name_str,
            "name_raw_hex": nd.name.hex(),
            "unk": nd.unk,
        }
        
        if nd.flag == 1:
            global_ref = name_str
            if map_assets and "objects" in map_assets:
                # Check for map-prefixed collision ID
                prefixed = f"{prefix}_{name_str}"
                if prefixed in map_assets["objects"]:
                    global_ref = prefixed
                elif name_str in map_assets["objects"]:
                    global_ref = name_str
            else:
                # Fallback hint
                global_ref = f"hint:{prefix}_{name_str}"
            
            entry["global_ref"] = global_ref
            
        name_entries.append(entry)
        
    result["obj_name_table"] = name_entries

    pos_groups = []
    pos_region_end = 0
    for i in range(ipd_hdr.pos_num):
        pos_off = ipd_hdr.obj_data_offset + i * IPDPosHeader.size()
        pos = IPDPosHeader.from_bytes(buf, pos_off)

        pos_json: dict = {
            "pos_header": _record_to_dict(pos, S.IPD_POS_HEADER_FIELDS),
        }

        unk2_blk_start = pos.unk2_offset
        unk2_blk_end   = unk2_blk_start + 8
        pos_json["unk2_block_hex"] = _hex_blob(buf, unk2_blk_start, unk2_blk_end)

        if pos.unk1_num > 0 and pos.unk1_offset != pos.unk2_offset:
            unk1_blk_start = pos.unk1_offset
            # Each unk1 entry is 8 bytes; unk1_num counts the entries.
            unk1_blk_end   = unk1_blk_start + pos.unk1_num * 8
            pos_json["unk1_block_hex"] = _hex_blob(buf, unk1_blk_start, unk1_blk_end)
        else:
            pos_json["unk1_block_hex"] = None

        obj_data_list = []
        for j in range(pos.obj_num):
            dta_off = pos.data_offset + j * IPDObjData.size()
            dta = IPDObjData.from_bytes(buf, dta_off)
            obj_data_list.append({
                "obj_id": dta.obj_id,
                "rt11": dta.rt11, "rt12": dta.rt12, "rt13": dta.rt13,
                "rt21": dta.rt21, "rt22": dta.rt22, "rt23": dta.rt23,
                "rt31": dta.rt31, "rt32": dta.rt32, "rt33": dta.rt33,
                "pad": dta.pad,
                "tx": dta.tx, "ty": dta.ty, "tz": dta.tz,
            })
            last = dta_off + IPDObjData.size()
            if last > pos_region_end:
                pos_region_end = last
        pos_json["obj_data"] = obj_data_list

        if unk2_blk_end > pos_region_end:
            pos_region_end = unk2_blk_end
        if pos.unk1_num > 0 and pos.unk1_offset != pos.unk2_offset:
            unk1_end = pos.unk1_offset + pos.unk1_num * 8
            if unk1_end > pos_region_end:
                pos_region_end = unk1_end

        pos_groups.append(pos_json)

    result["pos_groups"] = pos_groups

    plm_start = ipd_hdr.plm_offset
    if plm_start > pos_region_end:
        if "ipd_collision_header" in result:
            hdr = result["ipd_collision_header"]
            
            payload = {}
            coll_ranges = []
            def read_array(ptr, count, record_class, fields):
                if count > 0 and ptr > 0:
                    coll_ranges.append((ptr, ptr + count * record_class.size()))
                arr = []
                for i in range(count):
                    off = ptr + i * record_class.size()
                    rec = record_class.from_bytes(buf, off)
                    d = _record_to_dict(rec, fields)
                    # Convert raw bytes fields to JSON
                    for k, v in d.items():
                        if isinstance(v, bytes):
                            d[k] = v.hex()
                    arr.append(d)
                return arr
                
            payload["splitVertices"] = read_array(hdr["ptr_splitVertices"], hdr["splitVertexCount"], IPDCollSVector, S.IPD_COLL_SVECTOR_FIELDS)
            payload["surfaces"] = read_array(hdr["ptr_surfaces"], hdr["surfaceCount"], IPDCollSurface, S.IPD_COLL_SURFACE_FIELDS)
            payload["subcells"] = read_array(hdr["ptr_subcells"], hdr["subcellCount"], IPDCollSubcell, S.IPD_COLL_SUBCELL_FIELDS)
            payload["unkBlock3"] = read_array(hdr["ptr_unkBlock3"], hdr["unkBlock3Count"], IPDCollUnk3, S.IPD_COLL_UNK3_FIELDS)
            
            grid_len = hdr["gridWidth"] * hdr["gridHeight"] * 4
            if hdr["ptr_grid"] > 0 and grid_len > 0:
                coll_ranges.append((hdr["ptr_grid"], hdr["ptr_grid"] + grid_len))
            # Keep raw hex for byte-exact roundtrip reconstruction
            payload["grid_hex"] = _hex_blob(buf, hdr["ptr_grid"], hdr["ptr_grid"] + grid_len)
            # Also decode as structured (s16 start, s16 end) pairs per broadphase cell.
            # Each entry indexes into block5 (ptr_28): block5[start..end) gives subcell indices.
            grid_parsed = []
            grid_raw = buf[hdr["ptr_grid"]:hdr["ptr_grid"] + grid_len]
            for ci in range(hdr["gridWidth"] * hdr["gridHeight"]):
                off = ci * 4
                if off + 4 <= len(grid_raw):
                    start_idx, end_idx = struct.unpack_from("<hh", grid_raw, off)
                    grid_parsed.append({"start": start_idx, "end": end_idx})
            payload["grid"] = grid_parsed
            
            payload["block5"] = read_array(hdr["ptr_block5"], hdr["block5Count"], IPDCollUnk5, S.IPD_COLL_UNK5_FIELDS)
            # block6 (ptr_2C): u8 per entry — surface indices, same stride as block5.
            payload["block6"] = read_array(hdr["ptr_block6"], hdr["block6Count"], IPDCollUnk6, S.IPD_COLL_UNK6_FIELDS)

            
            merged = _merge_ranges(coll_ranges)
            
            gaps = []
            cursor = pos_region_end
            for start, end in merged:
                if start > cursor:
                    gaps.append((cursor, start))
                cursor = max(cursor, end)
            if cursor < plm_start:
                gaps.append((cursor, plm_start))
            
            gap_list = []
            for start, end in gaps:
                gap_list.append({
                    "start": start,
                    "end": end,
                    "hex": _hex_blob(buf, start, end)
                })
            payload["gaps"] = gap_list
            
            result["ipd_collision_payload"] = payload
        else:
            result["gap_obj_data_to_plm"] = _hex_blob(buf, pos_region_end, plm_start)
    else:
        result["gap_obj_data_to_plm"] = ""

    # Embedded PLM section (IPD's own geometry)
    result["ipd_plm"] = serialise_plm(buf, ipd_hdr.plm_offset, ipd_path.name)
    
    # Do NOT parse or embed _GLB.PLM geometry in this lean JSON.
    result["glb_plm"] = None

    return result


def local_json_to_ipd(json_path: Path) -> bytes:
    """
    Read a lean local JSON file produced by ipd_to_local_json.py and
    reconstruct the original binary IPD bytes dynamically.
    
    This ensures that expanding/contracting arrays in JSON do not overwrite
    adjacent data blocks in the binary file, while exactly preserving original
    gaps and offsets if the file hasn't been expanded!
    """
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    source_file = data["source_file"]
    print(f"\nReconstructing IPD: {source_file} (dynamic layout)")

    out = bytearray()

    def align4():
        rem = len(out) % 4
        if rem != 0:
            out.extend(b'\x00' * (4 - rem))

    def write_block(original_offset: int, block_data: bytes) -> int:
        """Write block at original_offset, or at len(out) if we expanded past it."""
        if len(out) > original_offset:
            align4()
        
        target = max(len(out), original_offset)
        if target > len(out):
            out.extend(b'\x00' * (target - len(out)))
            
        actual = len(out)
        out.extend(block_data)
        return actual

    def write_gap(original_offset: int, gap_data: bytes):
        """Write a gap. If we expanded into it, shrink it. If we expanded past it, drop it."""
        if len(out) < original_offset:
            out.extend(b'\x00' * (original_offset - len(out)))
        original_end = original_offset + len(gap_data)
        if len(out) < original_end:
            start_idx = max(0, len(out) - original_offset)
            out.extend(gap_data[start_idx:])

    # 1. IPD Header and Collision Header are fixed at 0 and 84
    write_block(0, b'\x00' * IPDFileHeader.size())
    if "ipd_collision_header" in data:
        write_block(84, b'\x00' * IPDCollHeader.size())
        
    items = []
    
    ipd_hdr_d = data["ipd_header"]
    
    # Object name table
    name_buf = bytearray()
    for entry in data["obj_name_table"]:
        name_buf.extend(IPDObjNameData(flag=entry["flag"], name=bytes.fromhex(entry["name_raw_hex"]), unk=entry["unk"]).to_bytes())
    items.append({"type": "block", "name": "obj_name_table", "orig": ipd_hdr_d["obj_name_offset"], "data": bytes(name_buf)})
    
    # Pos Headers array placeholder
    pos_hdr_size = len(data["pos_groups"]) * IPDPosHeader.size()
    items.append({"type": "block", "name": "pos_headers_array", "orig": ipd_hdr_d["obj_data_offset"], "data": b'\x00' * pos_hdr_size})
    
    # Pos Groups blocks
    for i, pg in enumerate(data["pos_groups"]):
        ph_d = pg["pos_header"]
        dta_buf = bytearray()
        for dta_d in pg["obj_data"]:
            dta_buf.extend(IPDObjData(
                obj_id=dta_d["obj_id"], rt11=dta_d["rt11"], rt12=dta_d["rt12"], rt13=dta_d["rt13"],
                rt21=dta_d["rt21"], rt22=dta_d["rt22"], rt23=dta_d["rt23"],
                rt31=dta_d["rt31"], rt32=dta_d["rt32"], rt33=dta_d["rt33"], pad=dta_d["pad"],
                tx=dta_d["tx"], ty=dta_d["ty"], tz=dta_d["tz"]
            ).to_bytes())
        if dta_buf: items.append({"type": "block", "name": f"pg_{i}_data", "orig": ph_d["data_offset"], "data": bytes(dta_buf)})
        if pg.get("unk1_block_hex"): items.append({"type": "block", "name": f"pg_{i}_unk1", "orig": ph_d["unk1_offset"], "data": bytes.fromhex(pg["unk1_block_hex"])})
        if pg.get("unk2_block_hex"): items.append({"type": "block", "name": f"pg_{i}_unk2", "orig": ph_d["unk2_offset"], "data": bytes.fromhex(pg["unk2_block_hex"])})

    # Collision Arrays
    if "ipd_collision_payload" in data:
        p = data["ipd_collision_payload"]
        c_orig = data["ipd_collision_header"]
        def add_c(name, arr, cls, orig_ptr):
            if not arr: return
            buf = bytearray()
            for d in arr: buf.extend(_dict_to_record(cls, d).to_bytes())
            items.append({"type": "block", "name": f"coll_{name}", "orig": orig_ptr, "data": bytes(buf)})
            
        add_c("splitVertices", p.get("splitVertices", []), IPDCollSVector, c_orig.get("ptr_splitVertices", 0))
        add_c("surfaces", p.get("surfaces", []), IPDCollSurface, c_orig.get("ptr_surfaces", 0))
        add_c("subcells", p.get("subcells", []), IPDCollSubcell, c_orig.get("ptr_subcells", 0))
        add_c("unkBlock3", p.get("unkBlock3", []), IPDCollUnk3, c_orig.get("ptr_unkBlock3", 0))
        if p.get("grid_hex"): items.append({"type": "block", "name": "coll_grid", "orig": c_orig.get("ptr_grid", 0), "data": bytes.fromhex(p["grid_hex"])})
        add_c("block5", p.get("block5", []), IPDCollUnk5, c_orig.get("ptr_block5", 0))
        add_c("block6", p.get("block6", []), IPDCollUnk6, c_orig.get("ptr_block6", 0))
        for gap in p.get("gaps", []): items.append({"type": "gap", "orig": gap["start"], "data": bytes.fromhex(gap["hex"])})
        
    # PLM Section
    original_plm_base = ipd_hdr_d["plm_offset"]
    plm_bytes = _encode_plm(data["ipd_plm"], original_plm_base, source_size=None)
    items.append({"type": "block", "name": "plm", "orig": original_plm_base, "data": plm_bytes})
    
    # Top-level gaps
    gap_h2n = data.get("gap_header_to_obj_name", "")
    if gap_h2n: items.append({"type": "gap", "orig": 84, "data": bytes.fromhex(gap_h2n)})
    gap_plm = data.get("gap_obj_data_to_plm", "")
    if gap_plm:
        gap_plm_data = bytes.fromhex(gap_plm)
        items.append({"type": "gap", "orig": original_plm_base - len(gap_plm_data), "data": gap_plm_data})

    # Sort and Execute
    items.sort(key=lambda x: x["orig"])
    
    actual_offsets = {}
    for item in items:
        if item["type"] == "block":
            actual_offsets[item["name"]] = write_block(item["orig"], item["data"])
        else:
            write_gap(item["orig"], item["data"])

    # Overwrite Pos Headers
    if "pos_headers_array" in actual_offsets:
        pos_hdr_start = actual_offsets["pos_headers_array"]
        for i, pg in enumerate(data["pos_groups"]):
            ph_d = pg["pos_header"]
            ph = IPDPosHeader(
                obj_num  = len(pg["obj_data"]),
                unk1_num = ph_d["unk1_num"],
                unk2_num = ph_d["unk2_num"],
                unk3_num = ph_d["unk3_num"],
                unk2     = ph_d["unk2"],
                unk3     = ph_d["unk3"],
                data_offset  = actual_offsets.get(f"pg_{i}_data", ph_d["data_offset"]),
                unk1_offset  = actual_offsets.get(f"pg_{i}_unk1", ph_d["unk1_offset"]),
                unk2_offset  = actual_offsets.get(f"pg_{i}_unk2", ph_d["unk2_offset"]),
            )
            _write_at(out, pos_hdr_start + i * IPDPosHeader.size(), ph.to_bytes())

    # Overwrite Collision Header
    if "ipd_collision_header" in data:
        c_orig = data["ipd_collision_header"]
        c_new = _dict_to_record(IPDCollHeader, c_orig)
        c_new.ptr_splitVertices = actual_offsets.get("coll_splitVertices", 84) - 84 if "coll_splitVertices" in actual_offsets else c_orig.get("ptr_splitVertices", 0)
        c_new.ptr_surfaces      = actual_offsets.get("coll_surfaces", 84) - 84 if "coll_surfaces" in actual_offsets else c_orig.get("ptr_surfaces", 0)
        c_new.ptr_subcells      = actual_offsets.get("coll_subcells", 84) - 84 if "coll_subcells" in actual_offsets else c_orig.get("ptr_subcells", 0)
        c_new.ptr_unkBlock3     = actual_offsets.get("coll_unkBlock3", 84) - 84 if "coll_unkBlock3" in actual_offsets else c_orig.get("ptr_unkBlock3", 0)
        c_new.ptr_grid          = actual_offsets.get("coll_grid", 84) - 84 if "coll_grid" in actual_offsets else c_orig.get("ptr_grid", 0)
        c_new.ptr_block5        = actual_offsets.get("coll_block5", 84) - 84 if "coll_block5" in actual_offsets else c_orig.get("ptr_block5", 0)
        c_new.ptr_block6        = actual_offsets.get("coll_block6", 84) - 84 if "coll_block6" in actual_offsets else c_orig.get("ptr_block6", 0)
        _write_at(out, 84, c_new.to_bytes())

    # Overwrite IPD Header
    ipd_hdr = _dict_to_record(IPDFileHeader, ipd_hdr_d)
    ipd_hdr.obj_name_offset = actual_offsets.get("obj_name_table", 0)
    ipd_hdr.obj_data_offset = actual_offsets.get("pos_headers_array", 0)
    ipd_hdr.plm_offset      = actual_offsets.get("plm", 0)
    _write_at(out, 0, ipd_hdr.to_bytes())
    
    out_bytes = bytes(out)
    original_size = data["source_size"]
    
    if len(out_bytes) < original_size:
        out_bytes += b'\x00' * (original_size - len(out_bytes))
        
    result = out_bytes
    print(f"  Reconstructed: {len(result)} bytes (Original: {original_size})")
    
    if len(result) > 45056:
        print(f"  WARNING: Output size {len(result)} exceeds the 45KB chunk buffer limit defined by 0xB000 in engine!")
        
    return result


