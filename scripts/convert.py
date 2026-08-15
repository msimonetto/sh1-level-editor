import argparse
import sys
import json
from pathlib import Path

# Core imports
from core.ipd_parser import serialise_local_ipd, local_json_to_ipd
from core.plm_parser import serialise_plm, _encode_plm

def cmd_ipd_json(args):
    ipd_path = args.ipd_file
    if not ipd_path.exists():
        print(f"File not found: {ipd_path}", file=sys.stderr)
        sys.exit(1)
    
    out_path = args.output if args.output else ipd_path.with_suffix(".json")
    
    map_assets = None
    if args.assets_dir:
        assets_dir = args.assets_dir
        prefix = ipd_path.stem.rstrip('0123456789ABCD')
        glb_json_path = assets_dir / f"{prefix}_GLB.json"
        if glb_json_path.exists():
            with open(glb_json_path, 'r', encoding='utf-8') as f:
                map_assets = json.load(f)
                
    result = serialise_local_ipd(ipd_path, map_assets)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=4)
    print(f"Exported IPD to {out_path.name}")

def cmd_json_ipd(args):
    json_path = args.json_file
    if not json_path.exists():
        print(f"File not found: {json_path}", file=sys.stderr)
        sys.exit(1)
        
    out_path = args.output if args.output else json_path.with_suffix(".IPD")
    try:
        ipd_data = local_json_to_ipd(json_path)
        out_path.write_bytes(ipd_data)
        print(f"Reconstructed IPD saved to {out_path.name}")
    except Exception as e:
        print(f"Failed to reconstruct IPD: {e}", file=sys.stderr)
        sys.exit(1)

def cmd_plm_json(args):
    plm_path = args.plm_file
    if not plm_path.exists():
        print(f"File not found: {plm_path}", file=sys.stderr)
        sys.exit(1)
        
    out_path = args.output if args.output else plm_path.with_suffix(".json")
    buf = plm_path.read_bytes()
    result = serialise_plm(buf, 0, plm_path.name)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=4)
    print(f"Exported PLM to {out_path.name}")

def cmd_json_plm(args):
    json_path = args.json_file
    if not json_path.exists():
        print(f"File not found: {json_path}", file=sys.stderr)
        sys.exit(1)
        
    out_path = args.output if args.output else json_path.with_suffix(".PLM")
    with open(json_path, "r", encoding="utf-8") as f:
        plm_json = json.load(f)
        
    base_offset = plm_json.get("base_offset", 0)
    plm_bytes = _encode_plm(plm_json, base_offset)
    
    with open(out_path, "wb") as f:
        f.write(plm_bytes)
    print(f"Reconstructed PLM to {out_path.name}")

def cmd_tim_png(args):
    from core.tim_to_png import convert_tim_to_png
    if not args.input_tim.is_file():
        raise SystemExit(f"ERROR: input file not found: {args.input_tim}")

    args.output_stem.parent.mkdir(parents=True, exist_ok=True)
    print(f"Converting {args.input_tim.name} ...")
    convert_tim_to_png(args.input_tim, args.output_stem)
    print("Done.")

def cmd_png_tim(args):
    from core.png_to_tim import convert_png_to_tim
    args.output_tim.parent.mkdir(parents=True, exist_ok=True)
    convert_png_to_tim(args.input_stem, args.output_tim)

def cmd_json_obj(args):
    from core.json_to_obj import extract_ipd_from_json
    json_path = args.json_file.resolve()
    assets_dir = args.assets_dir.resolve()
    out_dir = (args.out_dir or json_path.parent).resolve()
    extract_ipd_from_json(json_path, assets_dir, out_dir, no_unused=args.no_unused)

def cmd_patch_face(args):
    from core.patch_json_face import main as patch_main
    # patch_json_face parses its own args directly from sys.argv, so we need to mock sys.argv
    # or just refactor patch_json_face.py quickly. Since we just moved it to core, we can
    # either call its main() assuming sys.argv is intact or we refactor it.
    # We will refactor patch_json_face to take args.
    pass

def main():
    parser = argparse.ArgumentParser(description="Silent Hill 1 unified format conversion tool.")
    subparsers = parser.add_subparsers(dest="command", required=True, help="Subcommands")

    # ipd-json
    p = subparsers.add_parser("ipd-json", help="Convert a SH1 .IPD file to a JSON intermediate")
    p.add_argument("ipd_file", type=Path, help="Path to the .IPD file")
    p.add_argument("--assets-dir", type=Path, help="Path to directory containing <PREFIX>_GLB.json")
    p.add_argument("--output", "-o", type=Path, help="Output JSON file path")
    p.set_defaults(func=cmd_ipd_json)

    # json-ipd
    p = subparsers.add_parser("json-ipd", help="Reconstruct a binary SH1 .IPD file from JSON")
    p.add_argument("json_file", type=Path, help="Path to the .json file")
    p.add_argument("--output", "-o", type=Path, help="Output .IPD file path")
    p.set_defaults(func=cmd_json_ipd)

    # plm-json
    p = subparsers.add_parser("plm-json", help="Convert a SH1 .PLM file to a JSON intermediate")
    p.add_argument("plm_file", type=Path, help="Path to the .PLM file")
    p.add_argument("--output", "-o", type=Path, help="Output JSON file path")
    p.set_defaults(func=cmd_plm_json)

    # json-plm
    p = subparsers.add_parser("json-plm", help="Convert a JSON intermediate back to a SH1 .PLM file")
    p.add_argument("json_file", type=Path, help="Path to the JSON file")
    p.add_argument("--output", "-o", type=Path, help="Output .PLM file path")
    p.set_defaults(func=cmd_json_plm)

    # tim-png
    p = subparsers.add_parser("tim-png", help="Convert a PS1 .TIM file to dual-PNG lossless format")
    p.add_argument("input_tim", type=Path, help="Path to the source .TIM file")
    p.add_argument("output_stem", type=Path, help="Output path stem WITHOUT extension")
    p.set_defaults(func=cmd_tim_png)

    # png-tim
    p = subparsers.add_parser("png-tim", help="Convert the dual-PNG format back into a PS1 .TIM file")
    p.add_argument("input_stem", type=Path, help="Input PNG stem without extension")
    p.add_argument("output_tim", type=Path, help="Output .TIM file path")
    p.set_defaults(func=cmd_png_tim)

    # json-obj
    p = subparsers.add_parser("json-obj", help="Convert a SH1 JSON intermediate back to OBJ/MTL/TGA")
    p.add_argument("json_file", type=Path, help="Path to the .json intermediate file")
    p.add_argument("--assets-dir", type=Path, default=Path("../../data/workspace/textures"), help="Path to textures assets dir")
    p.add_argument("--out-dir", type=Path, help="Output directory for OBJ/MTL/TGA")
    p.add_argument("--no-unused", action="store_true", help="Skip the unused-mesh pass")
    p.set_defaults(func=cmd_json_obj)

    # patch-face
    p = subparsers.add_parser("patch-face", help="Patch a specific face in an IPD JSON file")
    p.add_argument("json_file", type=Path, help="Path to the JSON file to modify")
    p.add_argument("--objName", type=str, required=True, help="Object name to find in obj_headers")
    p.add_argument("--mesh", type=int, required=True, help="Mesh index")
    p.add_argument("--face", type=int, required=True, help="Face index")
    p.add_argument("--tex", type=str, help="New texture name (without .TIM)")
    p.add_argument("--clut", type=int, help="New CLUT row")
    p.add_argument("--uv", type=float, nargs=8, help="New UVs: u0 v0 u1 v1 u2 v2 u3 v3")
    p.set_defaults(func=cmd_patch_face)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
