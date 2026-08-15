"""
json_to_obj.py  --  JSON intermediate to OBJ/MTL/TGA converter.

WHAT IT DOES
  Reads the JSON file produced by ipd_to_json.py and reconstructs:
    <stem>.OBJ  -- vertex positions + face indices + UV + normals
    <stem>.MTL  -- material definitions referencing TGA files
    <stem>.TGA  -- per-texture baked images (decoded from companion .TIM files)

DESIGN CONTRACT
  The OBJ/MTL/TGA output MUST be byte-for-byte identical to the output of
  ipd_to_obj.py (the Phase 1a reference converter).  Any divergence is a
  bug -- we track this by SHA256 comparison in the validation script.

ALGORITHM
  This is a faithful port of the same geometry/texturing algorithm used in
  ipd_to_obj.py, but data now comes from the JSON intermediate rather than
  from binary structs.  The algorithm itself is unchanged so the output
  will be identical.

USAGE
  python json_to_obj.py <path/to/file.json>  [--no-unused]
  # Outputs <stem>.OBJ and <stem>.MTL next to the JSON file.
  # TIM files are looked up from the original IPD directory (source_file dir).
  # Pass --tim-dir <dir> to override the TIM lookup directory.
"""
from __future__ import annotations

import sys
import json
import struct
import argparse
import ctypes
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    from PIL import Image
except (ImportError, OSError):
    # Blender's native JSON importer only needs the PLM data classes below;
    # requiring Pillow at module import time prevents that path from working
    # when Blender's bundled Python does not expose Pillow correctly.
    Image = None


# ---------------------------------------------------------------------------
# Constants (identical to ipd_to_obj.py)
# ---------------------------------------------------------------------------
SCALE: float = 0.00390625  # == 1/256
MAP_MAX: int   = 10240


def f32(val: float) -> float:
    return ctypes.c_float(val).value


# ---------------------------------------------------------------------------
# PLM object / mesh model, loaded from JSON
# ---------------------------------------------------------------------------

class JsonPLMPack:
    """A polygon packet loaded from JSON."""
    __slots__ = (
        "u0", "v0", "cba", "u1", "v1", "unk1", "tex_num_and_unk2_byte",
        "u2", "v2", "u3", "v3",
        "faces_0", "faces_1", "faces_2", "faces_3",
        "normals_0", "normals_1", "normals_2", "normals_3",
    )

    def __init__(self, d: dict):
        for k in self.__slots__:
            setattr(self, k, d[k])

    @property
    def tex_num(self) -> int:
        return self.tex_num_and_unk2_byte & 0x7F

    @property
    def unk2(self) -> int:
        return (self.tex_num_and_unk2_byte >> 7) & 0x1

    @property
    def faces_3_raw(self) -> int:
        return self.faces_3


class JsonPLMMesh:
    """A single mesh from a PLM object, loaded from JSON."""
    def __init__(self, d: dict):
        dh = d["data_header"]
        self.pack_num  = dh["pack_num"]
        self.vert_num  = dh["vert_num"]
        self.num_c     = dh["num_c"]
        self.num_d     = dh["num_d"]
        # Note: offsets from data_header not needed since all data is inline
        self.vertices_xy: list[list[int]] = d["vertices_xy"]
        self.vertices_z:  list[int]       = d["vertices_z"]
        self.normals:     list[list[int]] = d["normals"]   # [[nx,ny,nz,pad], ...]
        self.packs: list[JsonPLMPack] = [JsonPLMPack(p) for p in d["packs"]]


class JsonPLMObj:
    """A PLM object header + meshes, loaded from JSON."""
    def __init__(self, d: dict):
        self.name: str = d["name"]
        self.mesh_num: int = d["mesh_num"]
        self.meshes: list[JsonPLMMesh] = [JsonPLMMesh(m) for m in d["meshes"]]


class JsonPLMSection:
    """The PLM section (embedded or GLB), loaded from JSON."""
    def __init__(self, d: dict):
        self.tex_names: list[str] = d["tex_names"]
        self.obj_headers: list[JsonPLMObj] = [JsonPLMObj(o) for o in d["obj_headers"]]

    def find_obj(self, name: str) -> tuple[int, JsonPLMObj] | None:
        for i, obj in enumerate(self.obj_headers):
            if obj.name == name:
                return i, obj
        return None


# ---------------------------------------------------------------------------
# TGA manager (identical logic to ipd_to_obj.py)
# ---------------------------------------------------------------------------

class PngBakeManager:
    def __init__(self, assets_dir: Path, out_dir: Path, prefix: str = ""):
        self.assets_dir = assets_dir
        self.out_dir = out_dir
        self.prefix = prefix
        self.out_pngs: Dict[str, Tuple[Image.Image, bytes, Image.Image]] = {}

    def get_or_create(self, name: str) -> Optional[Tuple[Image.Image, bytes, Image.Image]]:
        if Image is None:
            raise RuntimeError("Pillow is required only for OBJ texture baking")
        if name in self.out_pngs:
            return self.out_pngs[name]
        
        if self.prefix:
            src_png_path = self.assets_dir / "textures" / f"{name}.png"
            clut_path = self.assets_dir / "textures" / f"{name}_cluts.png"
        else:
            src_png_path = self.assets_dir / f"{name}.png"
            clut_path = self.assets_dir / f"{name}_cluts.png"

        if not src_png_path.exists():
            return None
            
        src_img = Image.open(src_png_path)
        if src_img.mode != "P" or not clut_path.exists():
            out_img = src_img.convert("RGBA")
            self.out_pngs[name] = (out_img, b'', out_img)
            return self.out_pngs[name]
            
        clut_img = Image.open(clut_path).convert("RGBA")
        src_pixels = src_img.tobytes()
        
        # The output image must start fully transparent (matching old TGA behavior)
        # Otherwise texture filtering might blend polygon edges with CLUT 0 colors.
        out_img = Image.new("RGBA", src_img.size, (0, 0, 0, 0))
        
        self.out_pngs[name] = (out_img, src_pixels, clut_img)
        return self.out_pngs[name]

    def flush_all(self):
        for name, (out_img, _, _) in self.out_pngs.items():
            png_path = self.out_dir / f"{name}.png"
            out_img.save(png_path, "PNG")



# ---------------------------------------------------------------------------
# OBJ writer (identical logic to ipd_to_obj.py)
# ---------------------------------------------------------------------------

class ObjWriter:
    def __init__(self, obj_path: Path, mtl_path: Path):
        self.obj_file = open(obj_path, "w", newline="\n")
        self.mtl_file = open(mtl_path, "w+", newline="\n")
        self.v_index:  int = 0
        self.vt_index: int = 1
        self.vn_index: int = 1
        self.mtllib_written = False
        self.written_materials = set()

    def write_to_file(self, text: str):
        self.obj_file.write(text)

    def write_mtllib(self, name: str):
        if not self.mtllib_written:
            self.write_to_file(f"mtllib {name}.MTL\n")
            self.mtllib_written = True

    def obj_comment(self, text: str):
        self.write_to_file(f"# {text}\n")

    def add_tex_to_mtl(self, tex_name: str, obj_name: str):
        if tex_name not in self.written_materials:
            self.mtl_file.write(f"newmtl {tex_name}_TEX \n \t map_Kd {tex_name}.png\n")
            self.written_materials.add(tex_name)

    def close(self):
        self.obj_file.close()
        self.mtl_file.close()





# ---------------------------------------------------------------------------
# Core geometry extraction (mirrors extract_object in ipd_to_obj.py exactly)
# ---------------------------------------------------------------------------

def extract_object_json(
    obj_name: str,
    plm: JsonPLMSection,
    pst: Optional[dict],           # the obj_data dict, or None for identity transform
    x_pos: int,
    y_pos: int,
    writer: ObjWriter,
    obj_counts: dict[int, int],
    png_mgr: PngBakeManager,
) -> int:
    """
    Extract geometry from a PLM object by name and write to OBJ.
    Mirrors extract_object() in ipd_to_obj.py exactly.
    Returns the object index in plm.obj_headers, or -1 if not found.
    """
    result = plm.find_obj(obj_name)
    if result is None:
        return -1
    idx, obj_hdr = result

    obj_fix_name = obj_name[:8]
    key = idx
    if key not in obj_counts:
        obj_counts[key] = 0

    map_max = f32(10240.0)
    scale   = f32(SCALE)
    y_fix   = 0

    for m in range(obj_hdr.mesh_num):
        dh = obj_hdr.meshes[m]
        packs = dh.packs

        cnt = obj_counts[key]
        writer.write_to_file(f"o {obj_fix_name}_{cnt//100}{(cnt//10 - cnt//100)}{cnt%10}\n")
        obj_counts[key] += 1

        # -- vertices
        for k in range(dh.vert_num):
            x, y = dh.vertices_xy[k]
            z    = dh.vertices_z[k]

            if pst is not None:
                tx_rot = pst["tx"] + int(pst["rt11"] * x / 0x1000) + int(pst["rt12"] * y / 0x1000) + int(pst["rt13"] * z / 0x1000)
                ty_rot = pst["ty"] + int(pst["rt21"] * x / 0x1000) + int(pst["rt22"] * y / 0x1000) + int(pst["rt23"] * z / 0x1000)
                tz_rot = pst["tz"] + int(pst["rt31"] * x / 0x1000) + int(pst["rt32"] * y / 0x1000) + int(pst["rt33"] * z / 0x1000)
                fx = f32(f32(tx_rot) + f32(map_max * x_pos)) * scale
                fy = f32(f32(ty_rot)) * scale
                fz = f32(f32(tz_rot) + f32(map_max * y_pos)) * scale
            else:
                fx = f32(f32(x + f32(map_max * x_pos)) * scale)
                fy = f32(y * scale)
                fz = f32(f32(z + f32(map_max * y_pos)) * scale)

            writer.write_to_file(f"v {f32(fx * -1):f} {f32(fy * -1):f} {f32(fz * 1):f}\n")

        no_tex = False
        old_tex_num = 0x7F

        # -- texture coordinates (and TGA baking)
        for pk in packs:
            clut_x = (pk.cba & 0x003F) << 4
            clut_y = (pk.cba & 0x7FC0) >> 6

            u0, v0 = pk.u0, pk.v0 + y_fix
            u1, v1 = pk.u1, pk.v1 + y_fix
            u2, v2 = pk.u2, pk.v2 + y_fix
            u3, v3 = pk.u3, pk.v3 + y_fix
            uv_data = [u0, v0, u1, v1, u2, v2, u3, v3]

            bake_data = None
            if pk.tex_num == 0x7F or pk.tex_num >= len(plm.tex_names):
                no_tex = True
                active_tex_name = None
            else:
                no_tex = False
                active_tex_name = plm.tex_names[pk.tex_num]
                bake_data = png_mgr.get_or_create(active_tex_name)
                if not bake_data:
                    no_tex = True

            if not no_tex and active_tex_name and bake_data:
                out_img, src_pixels, clut_img = bake_data
                tim_width, tim_height = out_img.size

                if src_pixels and clut_img:
                    x_max, x_min = 0, 256
                    y_max, y_min = 0, 256
                    for uv_i in range(4):
                        u, v = uv_data[2*uv_i], uv_data[2*uv_i+1]
                        if u > x_max: x_max = u
                        if u < x_min: x_min = u
                        if v > y_max: y_max = v
                        if v < y_min: y_min = v

                    if x_max >= tim_width:  x_max = tim_width - 1
                    if y_max >= tim_height: y_max = tim_height - 1
                    width  = x_max - x_min + 1
                    height = y_max - y_min + 1

                    if width > 1 and height > 1 and (width + x_min) <= tim_width and (height + y_min) <= tim_height:
                        stp = pk.unk2
                        out_pixels = out_img.load()
                        clut_pixels = clut_img.load()
                        
                        row_idx = min(clut_y, clut_img.height - 1)

                        for by in range(height):
                            src_y = y_min + by
                            for bx in range(width):
                                src_x = x_min + bx
                                idx = src_pixels[src_y * tim_width + src_x]
                                
                                pal_idx = clut_x + idx
                                if pal_idx >= clut_img.width:
                                    pal_idx = clut_img.width - 1
                                r, g, b, a = clut_pixels[pal_idx, row_idx]

                                if stp:
                                    if a == 0: d_a = 0
                                    elif a == 128: d_r=d_g=d_b=0; d_a = 127
                                    elif a == 255: d_a = 127
                                    else: d_a = 255
                                else:
                                    if a == 0: d_a = 0
                                    else: d_a = 255
                                    
                                out_pixels[src_x, src_y] = (r, g, b, d_a)

                writer.write_to_file(f"vt {f32(u0 / tim_width):f} {f32(1.0 - v0 / tim_height):f}\n")
                writer.write_to_file(f"vt {f32(u1 / tim_width):f} {f32(1.0 - v1 / tim_height):f}\n")
                writer.write_to_file(f"vt {f32(u2 / tim_width):f} {f32(1.0 - v2 / tim_height):f}\n")
                if pk.faces_3 != 0xFF:
                    writer.write_to_file(f"vt {f32(u3 / tim_width):f} {f32(1.0 - v3 / tim_height):f}\n")

        old_tex_num = 0x7F

        # -- normals (read from JSON normals array using pack's normals indices)
        for pk in packs:
            for k in range(3):
                n_idx = [pk.normals_0, pk.normals_1, pk.normals_2, pk.normals_3][k]
                if n_idx < len(dh.normals):
                    nx, ny, nz, _pad = dh.normals[n_idx]
                else:
                    nx, ny, nz = 0, 0, 0
                writer.write_to_file(f"vn {f32(f32(nx * scale) * -1):f} {f32(f32(ny * scale) * -1):f} {f32(f32(nz * scale) * 1):f}\n")
            if pk.faces_3 != 0xFF:
                n_idx = pk.normals_3
                if n_idx < len(dh.normals):
                    nx, ny, nz, _pad = dh.normals[n_idx]
                else:
                    nx, ny, nz = 0, 0, 0
                writer.write_to_file(f"vn {f32(f32(nx * scale) * -1):f} {f32(f32(ny * scale) * -1):f} {f32(f32(nz * scale) * 1):f}\n")

        old_tex_num = 0x7F

        # -- faces
        for pk in packs:
            if pk.tex_num != 0x7F and pk.tex_num < len(plm.tex_names):
                if pk.tex_num != old_tex_num:
                    tex_name = plm.tex_names[pk.tex_num]
                    writer.write_to_file(f"usemtl {tex_name}_TEX\n")
                    writer.add_tex_to_mtl(tex_name, obj_fix_name)
                    old_tex_num = pk.tex_num

                vbase  = writer.v_index + 1
                vtbase = writer.vt_index
                vnbase = writer.vn_index

                f_str = f"f {pk.faces_2 + vbase}/{vtbase + 2}/{vnbase + 2} {pk.faces_1 + vbase}/{vtbase + 1}/{vnbase + 1} {pk.faces_0 + vbase}/{vtbase + 0}/{vnbase + 0} \n"
                writer.write_to_file(f_str)
                if pk.faces_3 != 0xFF:
                    f_str2 = f"f {pk.faces_3 + vbase}/{vtbase + 3}/{vnbase + 3} {pk.faces_1 + vbase}/{vtbase + 1}/{vnbase + 1} {pk.faces_2 + vbase}/{vtbase + 2}/{vnbase + 2} \n"
                    writer.write_to_file(f_str2)
                    writer.vt_index += 1
                    writer.vn_index += 1
                writer.vt_index += 3
                writer.vn_index += 3
            else:
                vbase  = writer.v_index + 1
                vnbase = writer.vn_index

                f_str = f"f {pk.faces_2 + vbase}//{vnbase + 2} {pk.faces_1 + vbase}//{vnbase + 1} {pk.faces_0 + vbase}//{vnbase + 0} \n"
                writer.write_to_file(f_str)
                if pk.faces_3 != 0xFF:
                    f_str2 = f"f {pk.faces_3 + vbase}//{vnbase + 3} {pk.faces_1 + vbase}//{vnbase + 1} {pk.faces_2 + vbase}//{vnbase + 2} \n"
                    writer.write_to_file(f_str2)
                    writer.vn_index += 1

                old_tex_num = 0x7F
                writer.vn_index += 3

        writer.v_index += dh.vert_num

    return idx


# ---------------------------------------------------------------------------
# Top-level IPD extractor from JSON (mirrors extract_ipd in ipd_to_obj.py)
# ---------------------------------------------------------------------------

def extract_ipd_from_json(
    json_path: Path,
    assets_dir: Path,
    out_dir: Path,
    no_unused: bool = False,
) -> None:
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    source_file = data["source_file"]
    stem = Path(source_file).stem  # e.g. "THR0000"

    ipd_hdr = data["ipd_header"]
    x_pos = ipd_hdr["x_pos"]
    y_pos = ipd_hdr["y_pos"]

    print(f"\nExtracting from JSON: {json_path.name}")
    print(f"  source: {source_file}")
    print(f"  pos_num={ipd_hdr['pos_num']}  obj_num={ipd_hdr['obj_num']}")
    print(f"  x_pos={x_pos}  y_pos={y_pos}")

    # Build PLM section objects
    ipd_plm_data = data["ipd_plm"]
    glb_plm_data = data.get("glb_plm")

    ipd_plm = JsonPLMSection(ipd_plm_data)
    glb_plm: Optional[JsonPLMSection] = JsonPLMSection(glb_plm_data) if glb_plm_data else None

    print(f"  IPD PLM: {len(ipd_plm.obj_headers)} objects, {len(ipd_plm.tex_names)} textures")
    if glb_plm:
        print(f"  GLB PLM: {len(glb_plm.obj_headers)} objects, {len(glb_plm.tex_names)} textures")
    else:
        print(f"  NOTE: no GLB PLM -- GLB-flagged objects will be skipped")



    # Set up writers
    png_mgr = PngBakeManager(assets_dir, out_dir)
    writer  = ObjWriter(out_dir / f"{stem}.OBJ", out_dir / f"{stem}.MTL")
    writer.write_mtllib(stem)

    obj_counts: dict[int, int] = {}
    ipd_obj_used: set[int] = set()

    name_entries = data["obj_name_table"]  # list of {flag, name, ...}

    for i, pg in enumerate(data["pos_groups"]):
        pos_hdr = pg["pos_header"]
        obj_num   = pos_hdr["obj_num"]
        unk1_num  = pos_hdr["unk1_num"]
        unk1_offset = pos_hdr["unk1_offset"]

        obj_data_list = pg["obj_data"]

        for j, dta in enumerate(obj_data_list):
            obj_id = dta["obj_id"]

            if obj_id < 0 or obj_id >= len(name_entries):
                print(f"  WARNING: group {i} inst {j}: obj_id={obj_id} out of range")
                continue

            objn = name_entries[obj_id]
            obj_name = objn["name"]

            # TREE02 leaf special case (mirrors main.c lines 289-305)
            if unk1_num > 0 and obj_name == "TREE02":
                # Read 8-byte unk1_block from JSON
                unk1_hex = pg.get("unk1_block_hex")
                if unk1_hex:
                    raw = bytes.fromhex(unk1_hex)
                    txyz = struct.unpack_from("<hhhh", raw)
                    leaf_pst = {
                        "obj_id": 0,
                        "rt11": 0x1000, "rt12": 0, "rt13": 0,
                        "rt21": 0, "rt22": 0x1000, "rt23": 0,
                        "rt31": 0, "rt32": 0, "rt33": 0x1000,
                        "pad": 0,
                        "tx": txyz[0], "ty": txyz[1], "tz": txyz[2],
                    }
                    if glb_plm is not None:
                        extract_object_json("LEAF_1", glb_plm, leaf_pst,
                                            x_pos, y_pos, writer, obj_counts, png_mgr)

            if objn["flag"] == 0:  # mesh from embedded IPD PLM
                ret = extract_object_json(obj_name, ipd_plm, dta,
                                          x_pos, y_pos, writer, obj_counts, png_mgr)
                if ret >= 0:
                    ipd_obj_used.add(ret)
            elif objn["flag"] == 1:  # mesh from GLB PLM
                if glb_plm is not None:
                    extract_object_json(obj_name, glb_plm, dta,
                                        x_pos, y_pos, writer, obj_counts, png_mgr)
            else:
                print(f"  WARNING: unknown obj flag {objn['flag']} for {obj_name!r}")

    # Pass 2: unused IPD PLM meshes (mirrors main.c "Searching for unused meshes")
    if not no_unused:
        writer.obj_comment("unused meshes (not referenced by any placed object)")
        for i, obj_hdr in enumerate(ipd_plm.obj_headers):
            if i not in ipd_obj_used:
                null_pst = {
                    "obj_id": 0,
                    "rt11": 0x1000, "rt12": 0, "rt13": 0,
                    "rt21": 0, "rt22": 0x1000, "rt23": 0,
                    "rt31": 0, "rt32": 0, "rt33": 0x1000,
                    "pad": 0, "tx": 0, "ty": 0, "tz": 0,
                }
                extract_object_json(obj_hdr.name, ipd_plm, null_pst,
                                    x_pos, y_pos, writer, obj_counts, png_mgr)

    writer.close()
    png_mgr.flush_all()

    print(f"  Total vertices written: {writer.v_index}")
    print(f"  v_index={writer.v_index}  vt_index={writer.vt_index}  vn_index={writer.vn_index}")
    print(f"  PNG textures written:   {len(png_mgr.out_pngs)}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------




def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert a SH1 decoupled JSON intermediate back to OBJ/MTL/TGA."
    )
    parser.add_argument("json_file", type=Path, help="Path to the .json intermediate file")
    parser.add_argument("--assets-dir", type=Path, default=Path("../../data/workspace/textures"), help="Path to textures assets dir")

    parser.add_argument(
        "--out-dir", type=Path, default=Path("../../data/workspace/blender"),
        help="Output directory for OBJ/MTL/TGA (default: ../../data/workspace/blender)"
    )
    parser.add_argument(
        "--no-unused", action="store_true",
        help="Skip the unused-mesh pass"
    )
    args = parser.parse_args()

    json_path = args.json_file.resolve()
    
    script_dir = Path(__file__).resolve().parent
    assets_dir = args.assets_dir.resolve()
    out_dir   = (args.out_dir or json_path.parent).resolve()

    extract_ipd_from_local_json(json_path, assets_dir, out_dir, no_unused=args.no_unused)

if __name__ == "__main__":
    main()
