import sys
import os
import shutil
import struct
from pathlib import Path
import argparse

sys.path.append(str(Path(__file__).resolve().parent.parent))
from core.deps import load_dependencies, save_dependencies

def extract_plm_textures(plm_path: Path) -> list[str]:
    """Parse a PLM file's header to extract texture (TIM) names."""
    if not plm_path.exists():
        return []
        
    with open(plm_path, 'rb') as f:
        buf = f.read()
        
    if len(buf) < 20:
        return []
        
    # PLMFileHeader is 20 bytes: <HBBiiii
    plm_id, flag, tex_num, tex_name_offset, obj_num, obj_start_offset, unk_data_offset = struct.unpack_from("<HBBiiii", buf, 0)
    
    if plm_id != 0x0630:
        return []
        
    tex_names = []
    for i in range(tex_num):
        off = tex_name_offset + i * 24
        if off + 24 <= len(buf):
            raw = buf[off:off+24]
            name = raw.rstrip(b'\x00').decode('ascii', errors='replace')
            if name:
                tex_names.append(name)
    return tex_names

def parse_ipd(ipd_path: Path) -> tuple[set[str], bool]:
    """Parse an IPD file to find embedded textures and whether it needs _GLB.PLM."""
    if not ipd_path.exists():
        return set(), False
        
    with open(ipd_path, 'rb') as f:
        buf = f.read()
        
    if len(buf) < 84:
        return set(), False
        
    # IPDFileHeader is 84 bytes: <BBbbiBBBB8sii52si
    fields = struct.unpack_from("<BBbbiBBBB8sii52si", buf, 0)
    plm_offset = fields[4]
    obj_num = fields[5]
    obj_name_offset = fields[10]
    
    # 1. Check for GLB requirement by looking at the object name table
    requires_glb = False
    for i in range(obj_num):
        off = obj_name_offset + i * 16
        if off + 16 <= len(buf):
            flag, name_bytes, unk = struct.unpack_from("<i8si", buf, off)
            if flag == 1:
                requires_glb = True
                break
                
    # 2. Check embedded PLM for textures
    tex_names = set()
    if plm_offset > 0 and plm_offset + 20 <= len(buf):
        plm_id, plm_flag, tex_num, tex_name_offset, p_obj_num, obj_start_offset, unk_data_offset = struct.unpack_from("<HBBiiii", buf, plm_offset)
        if plm_id == 0x0630:
            abs_tex_offset = plm_offset + tex_name_offset
            for i in range(tex_num):
                off = abs_tex_offset + i * 24
                if off + 24 <= len(buf):
                    raw = buf[off:off+24]
                    name = raw.rstrip(b'\x00').decode('ascii', errors='replace')
                    if name:
                        tex_names.add(name)
                        
    return tex_names, requires_glb

def extract_chunk(ipd_name: str, complete_dir: Path, out_dir: Path, skip_deps: bool = False):
    if not ipd_name.upper().endswith(".IPD"):
        ipd_name += ".IPD"
        
    source_dir = complete_dir / "BG" if (complete_dir / "BG").exists() else complete_dir
        
    source_ipd = source_dir / ipd_name
    if not source_ipd.exists():
        print(f"Error: {ipd_name} not found in {source_dir}")
        return

    stem = source_ipd.stem
    if len(stem) > 4 and all(c in '0123456789abcdefABCDEF' for c in stem[-4:]):
        prefix = stem[:-4]
    else:
        prefix = stem

    workspace_dir = out_dir
    chunks_dir = workspace_dir / "chunks"
    geom_dir = workspace_dir / "geometry"
    tex_dir = workspace_dir / "textures"
    misc_dir = workspace_dir / "misc"
    
    chunks_dir.mkdir(parents=True, exist_ok=True)
    geom_dir.mkdir(parents=True, exist_ok=True)
    tex_dir.mkdir(parents=True, exist_ok=True)
    misc_dir.mkdir(parents=True, exist_ok=True)
    
    target_ipd = chunks_dir / ipd_name
    
    deps = {"textures": [], "geometry": []}
    
    print(f"[{stem}] Copying base IPD...")
    shutil.copy2(source_ipd, target_ipd)
    
    # Parse the IPD natively
    print(f"[{stem}] Parsing IPD for dependencies...")
    tex_names, requires_glb = parse_ipd(target_ipd)
    
    # Resolve PLM
    if requires_glb:
        glb_plm_name = f"{prefix}_GLB.PLM"
        glb_plm_path = source_dir / glb_plm_name
        if glb_plm_path.exists():
            print(f"[{stem}]   -> Found required GLB PLM: {glb_plm_name}")
            if not skip_deps:
                shutil.copy2(glb_plm_path, geom_dir / glb_plm_name)
            deps["geometry"].append(glb_plm_name)
            
            # Parse GLB PLM for additional textures
            glb_tex_names = extract_plm_textures(glb_plm_path)
            for t in glb_tex_names:
                tex_names.add(t)
        else:
            print(f"[{stem}]   -> WARNING: Required GLB PLM {glb_plm_name} not found in source directory!")
    else:
        print(f"[{stem}]   -> No external GLB PLM required.")
        
    # Resolve TIMs
    resolved_tims = 0
    for tex_name in sorted(list(tex_names)):
        tim_name = f"{tex_name}.TIM"
        tim_path = source_dir / tim_name
        target_path = tex_dir / tim_name
        if tim_path.exists():
            print(f"[{stem}]   -> Found texture: {tim_name}")
            if not skip_deps:
                shutil.copy2(tim_path, target_path)
            deps["textures"].append(tim_name)
            resolved_tims += 1
        else:
            print(f"[{stem}]   -> WARNING: Texture {tim_name} not found in source directory!")

    # Basic heuristic to copy related BIN files (for collision or other map data)
    prefix_2 = stem[:2]
    prefix_3 = stem[:3]
    if not skip_deps:
        for bin_file in source_dir.glob("*.BIN"):
            if bin_file.name.startswith(prefix_3) or bin_file.name.startswith(prefix_2):
                target_bin = misc_dir / bin_file.name
                if not target_bin.exists():
                    print(f"[{stem}]   -> Copying potentially related BIN: {bin_file.name}")
                    shutil.copy2(bin_file, target_bin)
                    
    # Save to dependencies.json
    deps_data = load_dependencies(workspace_dir)
    deps_data[stem] = deps
    save_dependencies(workspace_dir, deps_data)

    print(f"[{stem}] Extraction complete ({resolved_tims} textures resolved)\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Extract an IPD chunk and all its required assets.")
    parser.add_argument("ipd_names", nargs='+', help="IPD names or prefixes to extract (e.g. THR0000 or SC)")
    parser.add_argument("--complete-dir", type=Path, default=Path("data/assets/BG"), help="Directory containing complete extracted files")
    parser.add_argument("--out-dir", type=Path, default=Path("data/workspace"), help="Directory to extract chunks into")
    parser.add_argument("--skip-dependencies", action="store_true", help="If set, only copies IPD and updates dependencies.json, but does not copy TIM/PLM/BIN files.")
    
    args = parser.parse_args()
    
    cwd = Path.cwd()
    # Handle relative paths appropriately
    complete_dir = args.complete_dir if args.complete_dir.is_absolute() else (cwd / args.complete_dir).resolve()
    out_dir = args.out_dir if args.out_dir.is_absolute() else (cwd / args.out_dir).resolve()
    
    if not complete_dir.exists():
        print(f"Error: {complete_dir} does not exist.")
        sys.exit(1)
        
    for arg in args.ipd_names:
        if arg.upper().endswith(".IPD"):
            extract_chunk(arg, complete_dir, out_dir, args.skip_dependencies)
        else:
            # Prefix search
            prefix = arg.upper()
            source_dir = complete_dir / "BG" if (complete_dir / "BG").exists() else complete_dir
            matches = list(source_dir.glob(f"{prefix}*.IPD"))
            if not matches:
                print(f"No IPDs found matching prefix: {prefix}")
            for match in matches:
                extract_chunk(match.name, complete_dir, out_dir, args.skip_dependencies)
