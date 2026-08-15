"""
build_map_assets.py -- Builds per-map [PREFIX]_GLB.json from PLM/TIM files.

Replaces the deprecated build_global_assets.py. Instead of a monolithic
master_assets.json, this produces one [PREFIX]_GLB.json per map prefix,
mirroring the PS1 DMA VRAM loading lifecycle (each map loads its own assets).

SCHEMA of [PREFIX]_GLB.json:
{
  "map_prefix": "THR",
  "glb_plm": "THR_GLB.PLM",
  "tex_order": ["THR0001F", "THR0002F", ...],   <- PLM tex_names ordering
  "objects": {
    "FLOR0000": { ...full PLM obj JSON as from ipd_to_json.serialise_plm... },
    ...
  },
  "textures": {
    "THR0001F": {
      "bpp": 4,
      "num_colors": 16,
      "pixel_width": 256,
      "pixel_height": 128,
      "vram_img_x": 320,
      "vram_img_y": 256,
      "width_words": 64,
      "height_words": 128,
      "num_cluts": 3,
      "vram_clut_x": 0,
      "vram_clut_y": 480,
      "png": "THR0001F.png",
      "cluts_png": "THR0001F_cluts.png"
    },
    ...
  }
}

USAGE:
  python build_map_assets.py --prefix THR
  python build_map_assets.py --prefix THR --source data/assets/BG --out-dir data/workspace/geometry

  # The --prefix filter is REQUIRED to avoid accidentally processing the entire
  # dataset (hundreds of files). Use exactly the map prefix (e.g. THR, SPR, SU).
"""
from __future__ import annotations

import sys
import json
import struct
import argparse
from pathlib import Path

# Add scripts directory to path to resolve core
sys.path.append(str(Path(__file__).resolve().parent.parent))

from core.plm_parser import serialise_plm
from core.models import TIMFileHeader, TIMClutHeader, TIMImgHeader


# ---------------------------------------------------------------------------
# TIM metadata reader (headers only -- no pixel decoding)
# ---------------------------------------------------------------------------

def _read_tim_metadata(tim_path: Path) -> dict:
    """
    Parse a .TIM file and return a metadata dict describing its layout.
    Does NOT decode any pixel data; reads only the header structs.
    """
    buf = tim_path.read_bytes()

    hdr = TIMFileHeader.from_bytes(buf, 0)
    if hdr.id != 0x10:
        raise ValueError(
            f"{tim_path.name}: invalid TIM magic 0x{hdr.id:02X}"
        )

    bpp_code = hdr.bpp_and_flags & 0x03
    has_clut = (hdr.bpp_and_flags & 0x08) != 0

    bpp_map = {0: 4, 1: 8, 2: 16, 3: 24}
    bpp = bpp_map.get(bpp_code, bpp_code)
    num_colors = {4: 16, 8: 256}.get(bpp, 0)

    offset = TIMFileHeader.size()
    meta: dict = {}

    if has_clut:
        clut_hdr = TIMClutHeader.from_bytes(buf, offset)
        meta["num_cluts"] = clut_hdr.height
        meta["vram_clut_x"] = clut_hdr.x
        meta["vram_clut_y"] = clut_hdr.y
        # clut_hdr.width = number of colours in one palette (16 or 256)
        offset += clut_hdr.clut_length
    else:
        meta["num_cluts"] = 0
        meta["vram_clut_x"] = None
        meta["vram_clut_y"] = None

    img_hdr = TIMImgHeader.from_bytes(buf, offset)

    # img_hdr.width is in 16-bit words (not pixels).
    # Pixel width depends on BPP:
    #   4-bit  → pixel_width = width_words * 4
    #   8-bit  → pixel_width = width_words * 2
    #   16-bit → pixel_width = width_words * 1
    if bpp == 4:
        pixel_width = img_hdr.width * 4
    elif bpp == 8:
        pixel_width = img_hdr.width * 2
    else:
        pixel_width = img_hdr.width

    meta.update({
        "bpp": bpp,
        "num_colors": num_colors,
        "pixel_width": pixel_width,
        "pixel_height": img_hdr.height,
        "vram_img_x": img_hdr.x,
        "vram_img_y": img_hdr.y,
        "width_words": img_hdr.width,
        "height_words": img_hdr.height,
    })

    # PNG output filenames (relative — the converter writes these)
    stem = tim_path.stem
    meta["png"] = f"{stem}.png"
    if bpp in (4, 8):
        meta["cluts_png"] = f"{stem}_cluts.png"
    else:
        meta["cluts_png"] = None

    return meta


# ---------------------------------------------------------------------------
# Main builder
# ---------------------------------------------------------------------------

def build_map_assets(
    source_dir: Path,
    out_dir: Path,
    prefix: str,
) -> None:
    """
    Build [PREFIX]_GLB.json for a single map prefix.

    Parameters
    ----------
    source_dir : Path
        Directory containing the raw game files (*.PLM, *.TIM, *.IPD).
    out_dir : Path
        Output directory for the generated JSON file.
    prefix : str
        Map prefix to process (e.g. "THR", "SPR", "SU"). Case-sensitive.
    """
    out_dir.mkdir(parents=True, exist_ok=True)

    glb_plm_name = f"{prefix}_GLB.PLM"
    glb_path = source_dir / glb_plm_name

    if not glb_path.is_file():
        print(f"WARNING: {glb_plm_name} not found in {source_dir}. Skipping.")
        return

    print(f"\n[{prefix}] Processing {glb_plm_name} ...")

    # ---- Parse GLB PLM → objects -------------------------------------------
    buf = glb_path.read_bytes()
    try:
        plm_json = serialise_plm(buf, 0, glb_plm_name)
    except Exception as exc:
        print(f"  ERROR parsing {glb_plm_name}: {exc}")
        return

    objects: dict = {}
    for obj in plm_json.get("obj_headers", []):
        name = obj["name"]
        objects[name] = obj

    tex_order: list[str] = plm_json.get("tex_names", [])

    print(f"  Objects : {len(objects)}")
    print(f"  Tex refs in PLM: {len(tex_order)}")

    # ---- Scan TIM files for this prefix ------------------------------------
    # Glob all *.TIM files whose stem starts with the prefix.
    # We record metadata only; actual PNG conversion is a separate step.
    tim_files = sorted(source_dir.glob(f"{prefix}*.TIM"))
    textures: dict = {}

    for tim_path in tim_files:
        stem = tim_path.stem
        try:
            meta = _read_tim_metadata(tim_path)
            textures[stem] = meta
        except Exception as exc:
            print(f"  WARNING: could not parse {tim_path.name}: {exc}")

    print(f"  TIM files found: {len(tim_files)}")

    # ---- Assemble output JSON ----------------------------------------------
    glb_json = {
        "map_prefix": prefix,
        "glb_plm": glb_plm_name,
        "tex_order": tex_order,
        "tex_names_raw_hex": plm_json.get("tex_names_raw_hex", []),
        "plm_header": plm_json.get("plm_header", {}),
        "objects": objects,
        "textures": textures,
    }

    out_path = out_dir / f"{prefix}_GLB.json"
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(glb_json, f, indent=2, ensure_ascii=False)

    print(f"  Written : {out_path.name}")
    print(f"    {len(objects)} objects, {len(textures)} textures")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    ap = argparse.ArgumentParser(
        description=(
            "Build a map-specific [PREFIX]_GLB.json from raw PLM/TIM files.\n"
            "--prefix is required to avoid accidentally processing the entire dataset."
        )
    )
    ap.add_argument(
        "--prefix",
        required=True,
        help="Map prefix to process, e.g. THR, SPR, SU. Case-sensitive.",
    )
    ap.add_argument(
        "--source",
        type=Path,
        default=Path("../../data/assets/BG"),
        help="Directory containing raw game files (default: ../../data/assets/BG)",
    )
    ap.add_argument(
        "--out-dir",
        type=Path,
        default=Path("../../data/workspace/geometry"),
        help="Output directory for generated JSON (default: ../../data/workspace/geometry)",
    )
    args = ap.parse_args()

    cwd = Path.cwd()
    src = args.source if args.source.is_absolute() else (cwd / args.source).resolve()
    dst = args.out_dir if args.out_dir.is_absolute() else (cwd / args.out_dir).resolve()

    if not src.is_dir():
        raise SystemExit(f"ERROR: source directory not found: {src}")

    build_map_assets(src, dst, args.prefix)
    print("\nDone.")
