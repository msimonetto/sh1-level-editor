"""
tim_to_png.py -- Converts a PS1 .TIM file into the dual-PNG lossless format.

Replaces the deprecated tim_to_tga.py. Uses Pillow for PNG I/O.

OUTPUT (4-bit or 8-bit indexed TIM):
  [stem].png
      8-bit Indexed PNG. Raw pixel indices stored verbatim. CLUT #0 is baked
      in as the internal palette so the file renders correctly in any viewer.
      Transparency (tRNS chunk):
        - Index 0: alpha = 0 (fully transparent) IFF all CLUTs agree that
          index 0 is 0x0000. Otherwise alpha = 127 to avoid silently hiding
          real colour data that exists in other CLUTs.
        - All other indices: alpha = 128 if STP bit set, else 255.

  [stem]_cluts.png
      32-bit RGBA PNG.  Width = colours-per-palette (16 or 256).
      Height = number of CLUTs in the file.  One pixel-row per CLUT.
      STP → alpha mapping (lossless round-trip):
        0x0000  (black, STP=0)  →  RGBA(0,   0,   0,   0)   transparent mask
        0x8000  (black, STP=1)  →  RGBA(0,   0,   0, 128)   semi-transparent black
        RGB≠0,  STP=0           →  RGBA(R, G, B, 255)        opaque
        RGB≠0,  STP=1           →  RGBA(R, G, B, 128)        semi-transparent

OUTPUT (16-bit direct-colour TIM):
  [stem].png
      32-bit RGBA PNG. Same STP → alpha mapping as above. Lossless.
      No _cluts.png is generated (no palette data to preserve).

USAGE:
  python tim_to_png.py <input.TIM> <data/workspace/textures/stem>
  # → writes data/workspace/textures/stem.png  (and data/workspace/textures/stem_cluts.png for indexed TIM)
"""
from __future__ import annotations

import sys
import struct
import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    raise SystemExit(
        "Pillow is required.  Install it with:  pip install pillow"
    )

# Add parsers directory to path
from core.models import TIMFileHeader, TIMClutHeader, TIMImgHeader


# ---------------------------------------------------------------------------
# PS1 colour helpers
# ---------------------------------------------------------------------------

def _word_to_rgba(word: int) -> tuple[int, int, int, int]:
    """
    Convert a PS1 15-bit colour word to (R, G, B, A) using the lossless
    STP → alpha encoding defined in json_workflow.md.

    The mapping is fully invertible: given (R, G, B, A) we can reconstruct
    the original 15-bit word without any information loss.
    """
    r = (word & 0x001F) * 255 // 31
    g = ((word & 0x03E0) >> 5) * 255 // 31
    b = ((word & 0x7C00) >> 10) * 255 // 31
    stp = (word >> 15) & 1

    if word == 0x0000:
        # Special case: transparent mask (black, STP=0)
        a = 0
    elif stp == 1:
        # STP=1 (which is the majority of colors) is now fully opaque
        a = 255
    else:
        # Normal pixel with STP=0 is encoded as 127
        a = 127

    return (r, g, b, a)


# ---------------------------------------------------------------------------
# Core conversion
# ---------------------------------------------------------------------------

def convert_tim_to_png(tim_path: Path, out_stem: Path) -> None:
    """
    Convert one .TIM file into the dual-PNG lossless format.

    Parameters
    ----------
    tim_path : Path
        Source .TIM file.
    out_stem : Path
        Output path stem WITHOUT extension, e.g. Path("data/workspace/textures/THR0001F").
        The function produces <stem>.png and, for indexed TIM, <stem>_cluts.png.
    """
    buf = tim_path.read_bytes()

    # ------------------------------------------------------------------ Header
    hdr = TIMFileHeader.from_bytes(buf, 0)
    if hdr.id != 0x10:
        raise ValueError(
            f"{tim_path.name}: invalid TIM magic 0x{hdr.id:02X} (expected 0x10)"
        )

    bpp = hdr.bpp_and_flags & 0x03       # 0=4-bit, 1=8-bit, 2=16-bit, 3=24-bit
    has_clut = (hdr.bpp_and_flags & 0x08) != 0
    offset = TIMFileHeader.size()

    # -------------------------------------------------------------------- CLUT
    clut_hdr = None
    # all_cluts[row] = list of raw u16 words for that palette
    all_cluts: list[list[int]] = []

    if has_clut:
        clut_hdr = TIMClutHeader.from_bytes(buf, offset)
        clut_data_start = offset + TIMClutHeader.size()
        clut_data_len = clut_hdr.clut_length - TIMClutHeader.size()
        clut_data = buf[clut_data_start: clut_data_start + clut_data_len]

        total_entries = clut_hdr.width * clut_hdr.height
        raw_words = struct.unpack_from(f"<{total_entries}H", clut_data, 0)

        for row in range(clut_hdr.height):
            start_idx = row * clut_hdr.width
            all_cluts.append(list(raw_words[start_idx: start_idx + clut_hdr.width]))

        offset += clut_hdr.clut_length

    # ------------------------------------------------------------------- Image
    img_hdr = TIMImgHeader.from_bytes(buf, offset)
    img_data_start = offset + TIMImgHeader.size()
    img_data_len = img_hdr.img_length - TIMImgHeader.size()
    img_data = buf[img_data_start: img_data_start + img_data_len]

    # --------------------------------------------------------- Pixel dimensions
    if bpp == 0:      # 4-bit indexed
        width = img_hdr.width * 4
        num_colors = 16
    elif bpp == 1:    # 8-bit indexed
        width = img_hdr.width * 2
        num_colors = 256
    elif bpp == 2:    # 16-bit direct colour
        width = img_hdr.width
        num_colors = 0
    else:
        raise NotImplementedError(
            f"{tim_path.name}: unsupported BPP mode {bpp} (24-bit not handled)"
        )

    height = img_hdr.height
    out_png = out_stem.parent / (out_stem.name + ".png")

    # ======================================================== 16-bit: RGBA PNG
    if bpp == 2:
        _write_16bit_rgba_png(img_data, width, height, out_png, tim_path.name)
        return

    # ====================================== 4-bit / 8-bit: dual-PNG workflow

    if not all_cluts:
        raise ValueError(
            f"{tim_path.name}: indexed TIM but no CLUT data found"
        )

    # ---- Determine index-0 alpha -----------------------------------------
    # If any CLUT other than CLUT #0 has a non-zero word at index 0, setting
    # alpha=0 for index 0 in the embedded palette would silently hide real
    # colour data from those CLUTs.  Guard against this with alpha=127.
    idx0_has_real_data = any(
        clut[0] != 0x0000 for clut in all_cluts[1:]
    )
    idx0_alpha = 127 if idx0_has_real_data else 0

    # ---- Build palette from CLUT #0 (flat R,G,B × 256 for Pillow) ---------
    clut0 = all_cluts[0]
    pal_flat: list[int] = []      # R,G,B interleaved, 256 entries
    pal_alpha: list[int] = []     # alpha per index, 256 entries

    for i in range(256):
        if i < num_colors:
            word = clut0[i]
            r = (word & 0x001F) * 255 // 31
            g = ((word & 0x03E0) >> 5) * 255 // 31
            b = ((word & 0x7C00) >> 10) * 255 // 31
            stp = (word >> 15) & 1

            if i == 0:
                # Index 0 transparency: use guard alpha if information could be lost
                # Keep the RGB from CLUT #0 so the colour is still visible at alpha=127
                a = idx0_alpha
            else:
                # Per json_workflow.md, all other colors in the indexed preview PNG
                # must have Alpha 255, regardless of their STP bit (which is losslessly
                # preserved in the _cluts.png instead).
                a = 255
        else:
            # Pad beyond the actual palette size
            r, g, b, a = 0, 0, 0, 255

        pal_flat.extend([r, g, b])
        pal_alpha.append(a)

    alpha_bytes = bytes(pal_alpha)

    # ---- Unpack pixel indices (top-down, no vertical flip) -----------------
    pixel_array = bytearray(width * height)

    if bpp == 0:  # 4-bit: two indices per byte, low nibble = left pixel
        stride = width // 2
        for y in range(height):
            src_row = img_data[y * stride: (y + 1) * stride]
            dst_off = y * width
            for x, byte_val in enumerate(src_row):
                pixel_array[dst_off + x * 2 + 0] = byte_val & 0x0F
                pixel_array[dst_off + x * 2 + 1] = (byte_val >> 4) & 0x0F
    else:  # 8-bit: one index per byte
        for y in range(height):
            src_row = img_data[y * width: (y + 1) * width]
            pixel_array[y * width: (y + 1) * width] = src_row

    # ---- Write indexed PNG -------------------------------------------------
    img = Image.frombytes("P", (width, height), bytes(pixel_array))
    img.putpalette(pal_flat)
    img.save(out_png, "PNG", transparency=alpha_bytes)

    clut_info = (
        f"idx0_alpha={'127 (loss guard)' if idx0_has_real_data else '0 (transparent)'}"
    )
    print(
        f"  [indexed] {out_png.name}  ({width}×{height}, {len(all_cluts)} CLUT(s), {clut_info})"
    )

    # ---- Write CLUT palette sheet (_cluts.png) -----------------------------
    _write_clut_sheet(all_cluts, clut_hdr.width, out_stem)


def _write_16bit_rgba_png(
    img_data: bytes, width: int, height: int, out_png: Path, source_name: str
) -> None:
    """Write a 16-bit direct-colour TIM as a 32-bit RGBA PNG (lossless)."""
    img = Image.new("RGBA", (width, height))
    pixels = img.load()

    for y in range(height):
        row_words = struct.unpack_from(f"<{width}H", img_data, y * width * 2)
        for x, word in enumerate(row_words):
            pixels[x, y] = _word_to_rgba(word)

    img.save(out_png, "PNG")
    print(f"  [16-bit]  {out_png.name}  ({width}×{height})")


def _write_clut_sheet(
    all_cluts: list[list[int]], clut_width: int, out_stem: Path
) -> None:
    """
    Write the CLUT palette sheet as a 32-bit RGBA PNG.
    Width = clut_width (16 or 256), Height = number of CLUTs.
    Each row is one CLUT; each pixel uses the lossless STP→alpha mapping.
    """
    num_cluts = len(all_cluts)
    clut_img = Image.new("RGBA", (clut_width, num_cluts))
    pixels = clut_img.load()

    for row_idx, clut_row in enumerate(all_cluts):
        for col_idx in range(clut_width):
            word = clut_row[col_idx] if col_idx < len(clut_row) else 0
            pixels[col_idx, row_idx] = _word_to_rgba(word)

    out_cluts = out_stem.parent / (out_stem.name + "_cluts.png")
    clut_img.save(out_cluts, "PNG")
    print(f"  [cluts]   {out_cluts.name}  ({clut_width}×{num_cluts})")


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    ap = argparse.ArgumentParser(
        description=(
            "Convert a PS1 .TIM file to the dual-PNG lossless format.\n"
            "Produces <stem>.png and (for indexed TIM) <stem>_cluts.png."
        )
    )
    ap.add_argument("input_tim", type=Path, help="Path to the source .TIM file")
    ap.add_argument(
        "output_stem", type=Path,
        help=(
            "Output path stem WITHOUT extension, e.g. 'data/workspace/textures/THR0001F'.\n"
            "The output directory must already exist."
        ),
    )
    args = ap.parse_args()

    if not args.input_tim.is_file():
        raise SystemExit(f"ERROR: input file not found: {args.input_tim}")

    args.output_stem.parent.mkdir(parents=True, exist_ok=True)
    print(f"Converting {args.input_tim.name} ...")
    convert_tim_to_png(args.input_tim, args.output_stem)
    print("Done.")
