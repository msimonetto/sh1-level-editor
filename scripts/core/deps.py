import json
from pathlib import Path

def load_dependencies(workspace_dir: Path) -> dict:
    deps_path = workspace_dir / "dependencies.json"
    if not deps_path.exists():
        return {}
    try:
        with open(deps_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except json.JSONDecodeError:
        return {}

def save_dependencies(workspace_dir: Path, data: dict):
    deps_path = workspace_dir / "dependencies.json"
    with open(deps_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=4)
        
def get_shared_files(deps_data: dict, exclude_chunks: list[str]) -> set[str]:
    """Returns a set of all files (textures, geometry) used by any chunk NOT in exclude_chunks."""
    shared = set()
    for chunk, data in deps_data.items():
        if chunk in exclude_chunks:
            continue
        for tex in data.get("textures", []):
            shared.add(tex)
        for geom in data.get("geometry", []):
            shared.add(geom)
    return shared
