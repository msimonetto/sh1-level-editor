"""
png_to_tim.py -- Converts the dual-PNG lossless format back into a PS1 .TIM file.

Acts as the exact inverse of tim_to_png.py, resulting in a byte-for-byte replica
of the original .TIM. No reference TIM is required because all SH1 .TIM files
use (0, 0) for their CLUT and Image VRAM coordinates.
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


def _rgba_to_word(r: int, g: int, b: int, a: int) -> int:
    """
    Reverse the STP -> alpha mapping to reconstruct the PS1 15-bit colour word.
    Follows the strict lossless conversion rules established in verify_clut_lossless.py.
    """
    # 5-bit colour channels (inverts (c * 255) // 31)
    r5 = (r * 31 + 127) // 255
    g5 = (g * 31 + 127) // 255
    b5 = (b * 31 + 127) // 255

    word = r5 | (g5 << 5) | (b5 << 10)

    if a <= 64:
        return 0x0000
    elif a >= 192:
        return word | 0x8000
    else:
        return word


def convert_png_to_tim(png_stem: Path, out_tim: Path) -> None:
    img_png_path = png_stem.with_suffix(".png")
    clut_png_path = png_stem.parent / (png_stem.name + "_cluts.png")

    if not img_png_path.is_file():
        raise FileNotFoundError(f"Image PNG not found: {img_png_path}")

    # Determine Bit Depth
    bpp = 2  # Default 16-bit
    has_clut = 0
    if clut_png_path.is_file():
        has_clut = 1
        with Image.open(clut_png_path) as clut_img:
            if clut_img.width == 16:
                bpp = 0  # 4-bit
            elif clut_img.width == 256:
                bpp = 1  # 8-bit
            else:
                raise ValueError(f"Invalid CLUT width: {clut_img.width} (must be 16 or 256)")

    # 1. TIMFileHeader
    bpp_and_flags = bpp | (has_clut << 3)
    hdr = TIMFileHeader(
        id=0x10,
        ver=0,
        pad1_0=0,
        pad1_1=0,
        bpp_and_flags=bpp_and_flags,
        pad2_0=0,
        pad2_1=0,
        pad2_2=0
    )
    tim_bytes = bytearray(hdr.to_bytes())

    # 2. TIMClutHeader & Data
    if has_clut:
        clut_img = Image.open(clut_png_path).convert("RGBA")
        width, height = clut_img.size
        
        # CLUT data size: width * height * 2 bytes
        clut_data_len = width * height * 2
        clut_hdr = TIMClutHeader(
            clut_length=TIMClutHeader.size() + clut_data_len,
            x=0,
            y=0,
            width=width,
            height=height
        )
        tim_bytes.extend(clut_hdr.to_bytes())

        pixels = clut_img.load()
        for y in range(height):
            for x in range(width):
                r, g, b, a = pixels[x, y]
                word = _rgba_to_word(r, g, b, a)
                tim_bytes.extend(struct.pack("<H", word))
        clut_img.close()

    # 3. TIMImgHeader & Data
    img = Image.open(img_png_path)
    width, height = img.size

    # Calculate number of 16-bit words per row (TIMImgHeader width)
    if bpp == 0:
        words_per_row = width // 4
    elif bpp == 1:
        words_per_row = width // 2
    else:
        words_per_row = width

    img_data_len = words_per_row * 2 * height

    img_hdr = TIMImgHeader(
        img_length=TIMImgHeader.size() + img_data_len,
        x=0,
        y=0,
        width=words_per_row,
        height=height
    )
    tim_bytes.extend(img_hdr.to_bytes())

    # Write Image Data
    if bpp == 2:
        img = img.convert("RGBA")
        pixels = img.load()
        for y in range(height):
            for x in range(width):
                r, g, b, a = pixels[x, y]
                word = _rgba_to_word(r, g, b, a)
                tim_bytes.extend(struct.pack("<H", word))
    else:
        # Indexed 4-bit or 8-bit
        # Image must be single-band 'P' or 'L'
        if img.mode not in ("P", "L"):
            raise ValueError(f"Indexed TIM requires an indexed PNG, got {img.mode}")
        pixel_array = img.tobytes()
        
        if bpp == 0:
            # 4-bit: pack two indices per byte.
            # Low nibble = left pixel.
            # Example: [idx0, idx1] -> (idx1 << 4) | idx0
            for i in range(0, len(pixel_array), 2):
                p1 = pixel_array[i] & 0x0F
                # Handle odd width just in case, though PS1 textures are usually even multiples
                p2 = pixel_array[i+1] & 0x0F if i+1 < len(pixel_array) else 0
                byte_val = (p2 << 4) | p1
                tim_bytes.append(byte_val)
        else:
            # 8-bit: one byte per pixel, directly write.
            tim_bytes.extend(pixel_array)

    img.close()

    # Save output
    out_tim.write_bytes(tim_bytes)
    print(f"Created {out_tim.name}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(
        description="Convert the dual-PNG format back into a PS1 .TIM file."
    )
    ap.add_argument("input_stem", type=Path, help="Input PNG stem without extension (e.g. data/workspace/textures/THR0001F)")
    ap.add_argument("output_tim", type=Path, help="Output .TIM file path")
    args = ap.parse_args()

    args.output_tim.parent.mkdir(parents=True, exist_ok=True)
    convert_png_to_tim(args.input_stem, args.output_tim)
