#!/usr/bin/env python3
"""Generate a compact native-size coverage font for the 3DS UI."""

import argparse
from pathlib import Path
import struct
import unicodedata

from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont


MAGIC = b"IMBF"
HEADER = struct.Struct("<4sHHHHI")

HAN_RANGES = (
    (0x3400, 0x4DBF),
    (0x4E00, 0x9FFF),
    (0xF900, 0xFAFF),
    (0x20000, 0x323AF),
)

KANA_RANGES = (
    (0x3040, 0x30FF),
    (0x31F0, 0x31FF),
    (0xFF66, 0xFF9F),
    (0x1AFF0, 0x1AFFF),
    (0x1B000, 0x1B16F),
)

HANGUL_RANGES = (
    (0x1100, 0x11FF),
    (0x3130, 0x318F),
    (0xA960, 0xA97F),
    (0xAC00, 0xD7AF),
)

def read_codepoints(path: Path) -> list[int]:
    values = {
        int(line) for line in path.read_text(encoding="ascii").splitlines()
        if line.strip()
    }
    if not values:
        raise ValueError(f"{path}: empty codepoint whitelist")
    invalid = [value for value in values if value < 0 or value > 0x10FFFF]
    if invalid:
        raise ValueError(f"{path}: invalid Unicode codepoint {invalid[0]}")
    return sorted(values)


def pack_bitmap(image: Image.Image, width: int, height: int,
                alpha_bits: int) -> bytes:
    row_bytes = ((width + 3) // 4 if alpha_bits == 2
                 else (width + 7) // 8)
    packed = bytearray(row_bytes * height)
    for y in range(height):
        for x in range(width):
            value = image.getpixel((x, y))
            if alpha_bits == 2:
                coverage = round(value * 3 / 255)
                offset = y * row_bytes + x // 4
                packed[offset] |= coverage << ((3 - (x & 3)) * 2)
            elif value:
                offset = y * row_bytes + x // 8
                packed[offset] |= 0x80 >> (x & 7)
    return bytes(packed)


def font_codepoints(path: Path) -> set[int]:
    font = TTFont(path, lazy=True)
    try:
        return set().union(*(
            table.cmap.keys() for table in font["cmap"].tables
            if table.isUnicode()
        ))
    finally:
        font.close()


def in_ranges(codepoint: int,
              ranges: tuple[tuple[int, int], ...]) -> bool:
    return any(first <= codepoint <= last for first, last in ranges)


def is_han_alignment_glyph(codepoint: int) -> bool:
    """Return whether the glyph is eligible for Han foot correction."""
    return in_ranges(codepoint, HAN_RANGES)


def is_kana_alignment_glyph(codepoint: int) -> bool:
    """Select full-size baseline-bearing kana for visible-foot alignment."""
    if not in_ranges(codepoint, KANA_RANGES):
        return False
    char = chr(codepoint)
    if not unicodedata.category(char).startswith("L"):
        return False
    name = unicodedata.name(char, "")
    if ("SMALL" in name or "ITERATION MARK" in name or
            "PROLONGED SOUND MARK" in name):
        return False
    # Hiragana/katakana HE is a deliberately floating chevron. Its voiced and
    # semi-voiced forms decompose to the same base and must stay with it.
    base = unicodedata.normalize("NFD", char)[0]
    return base not in ("へ", "ヘ")


def align_japanese_visible_bottom(
        image: Image.Image, codepoint: int,
        target_bottom: int | None) -> Image.Image:
    """Align only glyphs whose natural form already reaches the text foot.

    Some Japanese letters intentionally float above the baseline (for
    example べ/ぺ), while 一 and ー are horizontal strokes in the middle of
    the em square. Moving those glyphs to the common visible bottom destroys
    the typeface's own vertical metrics.  A glyph therefore participates only
    when its original raster already reaches the row immediately above the
    requested common foot.
    """
    if target_bottom is None:
        return image
    align_han = is_han_alignment_glyph(codepoint)
    align_kana = is_kana_alignment_glyph(codepoint)
    if not align_han and not align_kana:
        return image
    bounds = image.getbbox()
    if bounds is None:
        return image
    visible_bottom = bounds[3] - 1
    if align_han and visible_bottom < target_bottom - 1:
        return image
    shift = target_bottom - visible_bottom
    if shift == 0:
        return image
    aligned = Image.new(image.mode, image.size, 0)
    aligned.paste(image, (0, shift))
    return aligned


def is_alphabetic_baseline_glyph(codepoint: int) -> bool:
    """Select alphabetic scripts without moving square CJK glyphs.

    At 24px, alphabetic baselines can quantize one row above the CJK em box.
    Moving the whole script group together preserves the typeface's own
    metrics while preventing mixed lines such as ``作词: KEI`` from looking
    uneven.  Han, kana and Hangul retain their native Noto CJK placement.
    """
    if (is_han_alignment_glyph(codepoint) or
            in_ranges(codepoint, KANA_RANGES) or
            in_ranges(codepoint, HANGUL_RANGES)):
        return False
    return unicodedata.category(chr(codepoint)).startswith("L") or (
        0x30 <= codepoint <= 0x39
    )


def shift_alphabetic_baseline(
        image: Image.Image, codepoint: int, shift: int) -> Image.Image:
    if shift == 0 or not is_alphabetic_baseline_glyph(codepoint):
        return image
    shifted = Image.new(image.mode, image.size, 0)
    shifted.paste(image, (0, shift))
    return shifted


def load_font(path: Path, pixels: int,
              weight: float) -> ImageFont.FreeTypeFont:
    font = ImageFont.truetype(str(path), pixels)
    try:
        axes = font.get_variation_axes()
        values = []
        for axis in axes:
            name = axis.get("name", b"")
            if isinstance(name, bytes):
                name = name.decode("ascii", "ignore")
            if "weight" in str(name).lower():
                values.append(max(axis["minimum"],
                                  min(axis["maximum"], weight)))
            else:
                values.append(axis["default"])
        if values:
            font.set_variation_by_axes(values)
    except (AttributeError, OSError):
        pass
    return font


def render_glyph(font: ImageFont.FreeTypeFont,
                 codepoint: int, width: int, height: int,
                 baseline: int,
                 japanese_visible_bottom: int | None,
                 alphabetic_baseline_shift: int,
                 alpha_bits: int) -> tuple[int, bytes]:
    char = chr(codepoint)
    image = Image.new("L" if alpha_bits == 2 else "1",
                      (width, height), 0)
    draw = ImageDraw.Draw(image)
    draw.text((0, baseline), char, font=font,
              fill=255 if alpha_bits == 2 else 1, anchor="ls")
    image = align_japanese_visible_bottom(
        image, codepoint, japanese_visible_bottom)
    image = shift_alphabetic_baseline(
        image, codepoint, alphabetic_baseline_shift)
    advance = round(font.getlength(char))
    advance = max(1, min(width, advance))
    return advance, pack_bitmap(image, width, height, alpha_bits)


def generate(font_path: Path, fallback_paths: list[Path],
             whitelist: Path, output: Path,
             font_pixels: int, width: int, height: int,
             baseline: int,
             japanese_visible_bottom: int | None,
             alphabetic_baseline_shift: int,
             alpha_bits: int, weight: float,
             include_font_coverage: bool) -> None:
    if (font_pixels <= 0 or width <= 0 or height <= 0 or
            baseline < 0 or baseline > height):
        raise ValueError("font and glyph dimensions must be positive")
    if (japanese_visible_bottom is not None and
            (japanese_visible_bottom < 0 or
             japanese_visible_bottom >= height)):
        raise ValueError(
            "Japanese visible bottom must be inside the glyph cell")
    if abs(alphabetic_baseline_shift) >= height:
        raise ValueError("alphabetic baseline shift must fit the glyph cell")
    font_paths = [font_path, *fallback_paths]
    fonts = [load_font(path, font_pixels, weight) for path in font_paths]
    coverages = [font_codepoints(path) for path in font_paths]
    codepoints = set(read_codepoints(whitelist))
    if include_font_coverage:
        for coverage in coverages:
            codepoints.update(coverage)
    codepoints = sorted(codepoints)
    if alpha_bits not in (1, 2):
        raise ValueError("alpha bits must be 1 or 2")
    bitmap_bytes = (((width + 3) // 4 if alpha_bits == 2
                     else (width + 7) // 8) * height)
    entry = struct.Struct(f"<IB3x{bitmap_bytes}s")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as stream:
        stream.write(HEADER.pack(
            MAGIC, 2 if alpha_bits == 2 else 1, width, height,
            bitmap_bytes, len(codepoints)))
        for codepoint in codepoints:
            font_index = next(
                (index for index, coverage in enumerate(coverages)
                 if codepoint in coverage),
                len(fonts) - 1,
            )
            advance, bitmap = render_glyph(
                fonts[font_index], codepoint, width, height, baseline,
                japanese_visible_bottom, alphabetic_baseline_shift,
                alpha_bits)
            stream.write(entry.pack(codepoint, advance, bitmap))
    expected = HEADER.size + len(codepoints) * entry.size
    if output.stat().st_size != expected:
        raise RuntimeError(f"{output}: generated size does not match format")
    print(
        f"wrote {output}: {len(codepoints):,} glyphs, "
        f"{output.stat().st_size:,} bytes, "
        f"{alpha_bits}-bit alpha, common font baseline"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a compact monochrome native-size font"
    )
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--fallback-font", type=Path, action="append",
                        default=[])
    parser.add_argument("--whitelist", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--font-pixels", type=int, default=24)
    parser.add_argument("--glyph-width", type=int, default=24)
    parser.add_argument("--glyph-height", type=int, default=32)
    parser.add_argument("--baseline", type=int, default=26)
    parser.add_argument(
        "--japanese-visible-bottom", type=int,
        help=("align baseline-bearing Han glyphs' last visible row to this "
              "shared foot; kana, punctuation and floating Han glyphs keep "
              "their native positions")
    )
    parser.add_argument(
        "--alphabetic-baseline-shift", type=int, default=0,
        help=("move alphabetic scripts and decimal digits together by this "
              "many raster rows; CJK, kana and Hangul keep native metrics")
    )
    parser.add_argument("--alpha-bits", type=int, choices=(1, 2), default=1)
    parser.add_argument("--weight", type=float, default=430.0)
    parser.add_argument(
        "--include-font-coverage", action="store_true",
        help=("include every Unicode codepoint supplied by the primary and "
              "fallback fonts in addition to the explicit whitelist")
    )
    args = parser.parse_args()
    generate(args.font, args.fallback_font, args.whitelist, args.output,
             args.font_pixels, args.glyph_width,
             args.glyph_height, args.baseline,
             args.japanese_visible_bottom,
             args.alphabetic_baseline_shift,
             args.alpha_bits, args.weight,
             args.include_font_coverage)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
