import sys
import os
from pathlib import Path
import argparse
import shutil

# Add parent directory to sys.path to import core
sys.path.append(str(Path(__file__).resolve().parent.parent))
from core.deps import load_dependencies, save_dependencies, get_shared_files

def clear_directory(dir_path: Path):
    if not dir_path.exists():
        return
    for item in dir_path.iterdir():
        if item.is_file() and item.suffix.upper() in [".IPD", ".PLM", ".TIM", ".BIN"]:
            try:
                item.unlink()
            except Exception as e:
                print(f"Failed to delete {item.name}: {e}")

def get_target_directories(target_type: str, workspace_dir: Path, override_dir: Path) -> dict:
    if target_type == "workspace":
        return {
            "chunks": workspace_dir / "chunks",
            "geometry": workspace_dir / "geometry",
            "textures": workspace_dir / "textures",
            "misc": workspace_dir / "misc"
        }
    else:
        bg_dir = override_dir / "BG"
        return {
            "chunks": bg_dir,
            "geometry": bg_dir,
            "textures": bg_dir,
            "misc": bg_dir
        }

def clear_all(target_type: str, workspace_dir: Path, override_dir: Path):
    print(f"Clearing entire {target_type}...")
    dirs = get_target_directories(target_type, workspace_dir, override_dir)
    
    # If workspace, we only clear specific subfolders. If override, they map to the same BG folder.
    # To avoid clearing BG 4 times, we use a set.
    unique_dirs = set(dirs.values())
    for d in unique_dirs:
        clear_directory(d)
        
    if target_type == "workspace":
        save_dependencies(workspace_dir, {})
        print("Reset dependencies.json to empty.")
        
    print(f"Successfully cleared {target_type}.")

def delete_selected(chunks: list[str], target_type: str, workspace_dir: Path, override_dir: Path, delete_textures: bool):
    deps_data = load_dependencies(workspace_dir)
    dirs = get_target_directories(target_type, workspace_dir, override_dir)
    
    # Calculate shared files
    shared_files = get_shared_files(deps_data, chunks)
    
    deleted_count = 0
    skipped_count = 0
    
    for chunk in chunks:
        print(f"[{chunk}] Processing deletion from {target_type}...")
        
        # 1. Delete IPD
        ipd_path = dirs["chunks"] / f"{chunk}.IPD"
        if ipd_path.exists():
            ipd_path.unlink()
            deleted_count += 1
            print(f"  -> Deleted {chunk}.IPD")
            
        # 2. Delete related BINs (heuristic)
        prefix2 = chunk[:2]
        prefix3 = chunk[:3]
        if dirs["misc"].exists():
            for bin_file in dirs["misc"].glob("*.BIN"):
                if bin_file.name.startswith(prefix3) or bin_file.name.startswith(prefix2):
                    bin_file.unlink()
                    deleted_count += 1
                    print(f"  -> Deleted {bin_file.name}")
                    
        # Process JSON dependencies (PLMs and TIMs)
        if chunk in deps_data:
            data = deps_data[chunk]
            
            # Geometry (PLM)
            for geom in data.get("geometry", []):
                if geom in shared_files:
                    print(f"  -> Skipped shared geometry: {geom}")
                    skipped_count += 1
                    continue
                geom_path = dirs["geometry"] / geom
                if geom_path.exists():
                    geom_path.unlink()
                    deleted_count += 1
                    print(f"  -> Deleted {geom}")
                    
            # Textures (TIM)
            for tex in data.get("textures", []):
                if not delete_textures:
                    print(f"  -> Skipped texture (flag disabled): {tex}")
                    skipped_count += 1
                    continue
                if tex in shared_files:
                    print(f"  -> Skipped shared texture: {tex}")
                    skipped_count += 1
                    continue
                tex_path = dirs["textures"] / tex
                if tex_path.exists():
                    tex_path.unlink()
                    deleted_count += 1
                    print(f"  -> Deleted {tex}")
                    
            # If target is workspace, remove from JSON tracking
            if target_type == "workspace":
                del deps_data[chunk]
                
    if target_type == "workspace":
        save_dependencies(workspace_dir, deps_data)
        print("Updated dependencies.json.")
        
    print(f"\nDeletion Complete: {deleted_count} deleted, {skipped_count} skipped.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Manage workspace/deployment deletions.")
    parser.add_argument("action", choices=["delete_selected", "clear_all"], help="Action to perform.")
    parser.add_argument("--target", choices=["workspace", "deployment"], required=True, help="Target directory to clean.")
    parser.add_argument("--chunks", nargs="*", default=[], help="List of chunks to delete (required for delete_selected).")
    parser.add_argument("--delete-textures", action="store_true", help="If set, unshared textures are deleted.")
    parser.add_argument("--workspace-dir", type=Path, default=Path("data/workspace"), help="Path to workspace root.")
    parser.add_argument("--override-dir", type=Path, default=Path("."), help="Path to deployment override root.")
    
    args = parser.parse_args()
    
    cwd = Path.cwd()
    workspace_dir = args.workspace_dir if args.workspace_dir.is_absolute() else (cwd / args.workspace_dir).resolve()
    override_dir = args.override_dir if args.override_dir.is_absolute() else (cwd / args.override_dir).resolve()
    
    if args.action == "clear_all":
        clear_all(args.target, workspace_dir, override_dir)
    elif args.action == "delete_selected":
        if not args.chunks:
            print("Error: --chunks must be provided for delete_selected.")
            sys.exit(1)
        delete_selected(args.chunks, args.target, workspace_dir, override_dir, args.delete_textures)
