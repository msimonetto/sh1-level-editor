import sys
import os
import shutil
import json
import hashlib
from pathlib import Path
import argparse

def get_sha256(filepath: Path) -> str:
    if not filepath.exists():
        return ""
    h = hashlib.sha256()
    with open(filepath, 'rb') as f:
        while chunk := f.read(8192):
            h.update(chunk)
    return h.hexdigest()

_asset_index_cache = None

def build_asset_index(assets_dir: Path) -> dict[str, Path]:
    global _asset_index_cache
    if _asset_index_cache is not None:
        return _asset_index_cache
    
    index = {}
    if assets_dir.exists():
        for path in assets_dir.rglob("*"):
            if path.is_file():
                index[path.name.upper()] = path
    _asset_index_cache = index
    return index

def find_asset_path(assets_dir: Path, filename: str) -> Path:
    """Finds original asset file by searching across all assets_dir subdirectories (BG, TIM, MISC, CHARA, ITEM, etc.)."""
    index = build_asset_index(assets_dir)
    if filename.upper() in index:
        return index[filename.upper()]
    
    bg_path = assets_dir / "BG" / filename
    if bg_path.exists():
        return bg_path
        
    return assets_dir / "BG" / filename

def deploy_chunks(chunks: list[str], workspace_dir: Path, assets_dir: Path, override_dir: Path):
    deps_path = workspace_dir / "dependencies.json"
    deps_data = {}
    
    if deps_path.exists():
        with open(deps_path, 'r', encoding='utf-8') as f:
            try:
                deps_data = json.load(f)
            except json.JSONDecodeError:
                print("Warning: Invalid dependencies.json file.")
                
    override_bg_dir = override_dir / "BG"
    override_bg_dir.mkdir(parents=True, exist_ok=True)
    
    files_to_check = set()
    
    for chunk in chunks:
        # 1. Base IPD file
        files_to_check.add(f"{chunk}.IPD")
        
        # 2. Files from dependencies.json
        if chunk in deps_data:
            data = deps_data[chunk]
            for tex in data.get("textures", []):
                files_to_check.add(tex)
            for geom in data.get("geometry", []):
                files_to_check.add(geom)
                
        # 3. Scan workspace directories for matching files across types (IPD, PLM, TIM, BIN)
        prefix_2 = chunk[:2]
        prefix_3 = chunk[:3]
        
        # Scan chunks directory
        chunks_dir = workspace_dir / "chunks"
        if chunks_dir.exists():
            for f in chunks_dir.glob("*.IPD"):
                if f.name.upper().startswith(chunk.upper()):
                    files_to_check.add(f.name)
            for f in chunks_dir.glob("*.ipd"):
                if f.name.upper().startswith(chunk.upper()):
                    files_to_check.add(f.name)
                    
        # Scan geometry directory (PLM)
        geom_dir = workspace_dir / "geometry"
        if geom_dir.exists():
            for f in geom_dir.glob("*.PLM"):
                if f.name.upper().startswith(prefix_3.upper()) or f.name.upper().startswith(prefix_2.upper()):
                    files_to_check.add(f.name)
                    
        # Scan textures directory (TIM)
        tex_dir = workspace_dir / "textures"
        if tex_dir.exists():
            for f in tex_dir.glob("*.TIM"):
                if f.name.upper().startswith(chunk.upper()) or f.name.upper().startswith(prefix_3.upper()):
                    files_to_check.add(f.name)
                    
        # Scan misc directory (BIN)
        misc_dir = workspace_dir / "misc"
        if misc_dir.exists():
            for f in misc_dir.glob("*.BIN"):
                if f.name.upper().startswith(prefix_3.upper()) or f.name.upper().startswith(prefix_2.upper()):
                    files_to_check.add(f.name)
                    
    copied = 0
    redundant = 0
    deleted = 0
    missing = 0
    
    print(f"Scanning {len(files_to_check)} unique files for deployment...")
    
    for filename in sorted(list(files_to_check)):
        fn_upper = filename.upper()
        # Locate workspace file
        if fn_upper.endswith(".IPD"):
            ws_path = workspace_dir / "chunks" / filename
            if not ws_path.exists():
                ws_path = workspace_dir / "chunks" / filename.lower()
        elif fn_upper.endswith(".PLM"):
            ws_path = workspace_dir / "geometry" / filename
        elif fn_upper.endswith(".TIM"):
            ws_path = workspace_dir / "textures" / filename
        elif fn_upper.endswith(".BIN"):
            ws_path = workspace_dir / "misc" / filename
        else:
            ws_path = workspace_dir / filename
            
        if not ws_path.exists():
            missing += 1
            continue
            
        # Locate original asset
        asset_path = find_asset_path(assets_dir, filename)
        target_path = override_bg_dir / filename
        
        ws_hash = get_sha256(ws_path)
        asset_hash = get_sha256(asset_path) if asset_path.exists() else ""
        
        if ws_hash != asset_hash:
            # File is modified compared to original game assets
            target_hash = get_sha256(target_path) if target_path.exists() else ""
            if target_hash != ws_hash:
                print(f"  -> [DEPLOY] {filename} has differences. Copying to deployment...")
                shutil.copy2(ws_path, target_path)
                copied += 1
            else:
                redundant += 1
        else:
            # File is unchanged from original game assets
            redundant += 1
            if target_path.exists():
                print(f"  -> [CLEANUP] {filename} matches original asset. Removing redundant deployment file...")
                target_path.unlink()
                deleted += 1
                
    print(f"\nDeployment Complete!")
    print(f"  Copied (Modified): {copied}")
    print(f"  Unchanged (Skipped): {redundant}")
    print(f"  Cleaned (Orphans): {deleted}")
    print(f"  Missing: {missing}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Smart deploy modified workspace chunks to the override directory.")
    parser.add_argument("chunks", nargs='+', help="List of chunks to deploy (e.g. THR0000).")
    parser.add_argument("--workspace-dir", type=Path, default=Path("data/workspace"), help="Path to the workspace root.")
    parser.add_argument("--assets-dir", type=Path, default=Path("data/assets"), help="Path to the extracted assets root.")
    parser.add_argument("--override-dir", type=Path, required=True, help="Path to the game's override directory.")
    
    args = parser.parse_args()
    
    cwd = Path.cwd()
    workspace_dir = args.workspace_dir if args.workspace_dir.is_absolute() else (cwd / args.workspace_dir).resolve()
    assets_dir = args.assets_dir if args.assets_dir.is_absolute() else (cwd / args.assets_dir).resolve()
    override_dir = args.override_dir if args.override_dir.is_absolute() else (cwd / args.override_dir).resolve()
    
    deploy_chunks(args.chunks, workspace_dir, assets_dir, override_dir)
