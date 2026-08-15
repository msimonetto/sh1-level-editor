"""
convert_all_tims.py -- Batch-converts a set of .TIM files to dual-PNG format.

Uses tim_to_png.convert_tim_to_png() to produce <stem>.png and
<stem>_cluts.png (or single RGBA PNG for 16-bit TIM) for each source file.

DEFAULT BEHAVIOUR:
  Converts all TIM files whose name starts with --prefix (default: THR) found
  in --source.  Writes PNG outputs to --out-dir/textures/.

  The prefix filter is intentional: processing the entire dataset
  (hundreds of TIM files across all map types) is expensive and unnecessary
  during development.  Use --prefix to target exactly the map you need.

USAGE:
  # Convert all THR*.TIM from the complete data directory
  python convert_all_tims.py

  # Convert SPR map TIMs
  python convert_all_tims.py --prefix SPR

  # Convert a specific subset
  python convert_all_tims.py --prefix THR --source path/to/complete --out-dir path/to/output
"""
from __future__ import annotations

import sys
import argparse
from pathlib import Path

# Add scripts directory to path to resolve core
sys.path.append(str(Path(__file__).resolve().parent.parent))

# Add converters dir to path
from core.tim_to_png import convert_tim_to_png


def batch_convert(source_dir: Path, out_dir: Path, prefix: str) -> None:
    """
    Batch-convert all TIM files matching the given prefix.

    Parameters
    ----------
    source_dir : Path
        Directory to scan for *.TIM files.
    out_dir : Path
        Base output directory. PNGs are written to out_dir/textures/.
    prefix : str
        File-name prefix filter (e.g. "THR" matches "THR0001F.TIM").
    """
    target_dir = out_dir / "textures"
    target_dir.mkdir(parents=True, exist_ok=True)

    tim_files = sorted(source_dir.glob(f"{prefix}*.TIM"))
    if not tim_files:
        print(f"No {prefix}*.TIM files found in {source_dir}.")
        return

    print(f"Found {len(tim_files)} {prefix}*.TIM file(s). Output -> {target_dir}\n")

    ok = 0
    errors = []
    for tim_path in tim_files:
        out_stem = target_dir / tim_path.stem
        try:
            print(f"Converting {tim_path.name} ...")
            convert_tim_to_png(tim_path, out_stem)
            ok += 1
        except Exception as exc:
            print(f"  ERROR: {exc}")
            errors.append((tim_path.name, str(exc)))

    print(f"\nComplete: {ok}/{len(tim_files)} converted successfully.")
    if errors:
        print(f"Errors ({len(errors)}):")
        for name, msg in errors:
            print(f"  {name}: {msg}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    ap = argparse.ArgumentParser(
        description=(
            "Batch-convert TIM files to dual-PNG lossless format.\n"
            "Defaults to the THR prefix to avoid processing the full dataset."
        )
    )
    ap.add_argument(
        "--prefix",
        default="THR",
        help="Map prefix to filter TIM files by (default: THR).",
    )
    ap.add_argument(
        "--source",
        type=Path,
        default=Path("../../../data/assets/BG"),
        help="Source directory containing .TIM files (default: data/assets/BG).",
    )
    ap.add_argument(
        "--out-dir",
        type=Path,
        default=Path("../../../data/workspace/textures"),
        help="Base output directory for PNG files (default: data/workspace/textures).",
    )
    args = ap.parse_args()

    # Resolve relative paths from CWD (the directory the user runs from),
    # not from the script file's own location.
    cwd = Path.cwd()
    src = args.source if args.source.is_absolute() else (cwd / args.source).resolve()
    dst = args.out_dir if args.out_dir.is_absolute() else (cwd / args.out_dir).resolve()

    if not src.is_dir():
        raise SystemExit(f"ERROR: source directory not found: {src}")

    batch_convert(src, dst, args.prefix)
