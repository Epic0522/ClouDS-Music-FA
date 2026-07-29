#!/usr/bin/env python3
"""Build the compact 512x512 GPU atlas used by the Frutiger Aero UI."""

from __future__ import annotations

import argparse
import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ATLAS_SIZE = 512
SUPERSAMPLE = 4


def add_fine_grain(image: Image.Image, amount: int, seed: int) -> Image.Image:
    randomizer = random.Random(seed)
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, alpha = pixels[x, y]
            grain = randomizer.randint(-amount, amount)
            pixels[x, y] = (
                max(0, min(255, red + grain)),
                max(0, min(255, green + grain)),
                max(0, min(255, blue + grain)),
                alpha,
            )
    return image


def glossy_tile(
    width: int,
    height: int,
    top: tuple[int, int, int],
    bottom: tuple[int, int, int],
    border: tuple[int, int, int, int],
    radius: int,
    seed: int,
    shadow_alpha: int = 150,
    shadow_offset: int = 2,
    shadow_blur: float = 3.0,
    inner_highlight_alpha: int = 112,
    top_line_alpha: int = 128,
    bottom_line_alpha: int = 72,
) -> Image.Image:
    target_size = (width, height)
    aa = SUPERSAMPLE
    width *= aa
    height *= aa
    radius *= aa
    tile = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    inset = max(5 * aa, radius // 3)
    shadow_mask = Image.new("L", (width, height), 0)
    ImageDraw.Draw(shadow_mask).rounded_rectangle(
        (inset, inset + shadow_offset * aa,
         width - inset - aa,
         height - inset - aa + shadow_offset * aa),
        radius=radius,
        fill=shadow_alpha,
    )
    shadow_mask = shadow_mask.filter(
        ImageFilter.GaussianBlur(radius=shadow_blur * aa)
    )
    shadow = Image.new("RGBA", (width, height), (55, 55, 51, 0))
    shadow.putalpha(shadow_mask)
    tile.alpha_composite(shadow)

    fill = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    pixels = fill.load()
    for y in range(height):
        ratio = y / max(1, height - 1)
        for x in range(width):
            highlight = 6 if y < height * 0.36 else 0
            pixels[x, y] = (
                min(255, int(top[0] * (1 - ratio) + bottom[0] * ratio) + highlight),
                min(255, int(top[1] * (1 - ratio) + bottom[1] * ratio) + highlight),
                min(255, int(top[2] * (1 - ratio) + bottom[2] * ratio) + highlight),
                250,
            )
    fill = add_fine_grain(fill, 3, seed)
    mask = Image.new("L", (width, height), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (inset, inset, width - inset - aa, height - inset - aa),
        radius=radius,
        fill=255,
    )
    tile.paste(fill, (0, 0), mask)
    draw = ImageDraw.Draw(tile)
    draw.rounded_rectangle(
        (inset, inset, width - inset - aa, height - inset - aa),
        radius=radius,
        outline=border,
        width=2 * aa,
    )
    draw.rounded_rectangle(
        (inset + 2 * aa, inset + 2 * aa,
         width - inset - 3 * aa, height - inset - 3 * aa),
        radius=max(2 * aa, radius - 4 * aa),
        outline=(255, 255, 255, inner_highlight_alpha),
        width=aa,
    )
    draw.line(
        (inset + radius, inset + 3 * aa,
         width - inset - radius - aa, inset + 3 * aa),
        fill=(255, 255, 255, top_line_alpha),
        width=aa,
    )
    draw.line(
        (inset + radius, height - inset - 3 * aa,
         width - inset - radius - aa, height - inset - 3 * aa),
        fill=(71, 83, 84, bottom_line_alpha),
        width=aa,
    )
    return tile.resize(target_size, Image.Resampling.LANCZOS)


def cover_inset_tile(dark: bool) -> Image.Image:
    """Flat artwork bed with no rim, grain or shadow to leak past the cover."""
    size = 32
    aa = SUPERSAMPLE
    tile = Image.new("RGBA", (size * aa, size * aa), (0, 0, 0, 0))
    inset = 5 * aa
    ImageDraw.Draw(tile).rounded_rectangle(
        (inset, inset, size * aa - inset - aa, size * aa - inset - aa),
        radius=9 * aa,
        fill=(72, 77, 79, 255) if dark else (250, 247, 239, 255),
    )
    return tile.resize((size, size), Image.Resampling.LANCZOS)


def button_glow_layer(width: int, height: int,
                      radius: int = 22) -> Image.Image:
    """Tintable inner light for a button, without a second rim or shadow."""
    target_size = (width, height)
    aa = SUPERSAMPLE
    width *= aa
    height *= aa
    radius *= aa
    inset = max(5 * aa, radius // 3) + aa
    layer = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    light = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    pixels = light.load()
    for y in range(height):
        ratio = y / max(1, height - 1)
        alpha = int(232 * (1.0 - ratio) + 190 * ratio)
        for x in range(width):
            pixels[x, y] = (255, 231, 126, alpha)
    mask = Image.new("L", (width, height), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (inset, inset, width - inset - aa, height - inset - aa),
        radius=max(2 * aa, radius - aa),
        fill=255,
    )
    layer.paste(light, (0, 0), mask)
    return layer.resize(target_size, Image.Resampling.LANCZOS)


def frosted_panel(width: int, height: int,
                  dark: bool = False) -> Image.Image:
    target_size = (width, height)
    aa = SUPERSAMPLE
    width *= aa
    height *= aa
    randomizer = random.Random(0xFA01)
    low_frequency = Image.new("L", (32, 16))
    low_pixels = low_frequency.load()
    for y in range(low_frequency.height):
        for x in range(low_frequency.width):
            low_pixels[x, y] = randomizer.randint(70, 190)
    low_frequency = low_frequency.resize(
        (width, height), Image.Resampling.BICUBIC
    ).filter(ImageFilter.GaussianBlur(radius=7.0 * aa))

    material = Image.new("RGBA", (width, height))
    pixels = material.load()
    frost = low_frequency.load()
    for y in range(height):
        vertical = y / max(1, height - 1)
        for x in range(width):
            haze = (frost[x, y] - 128) // 11
            if dark:
                pixels[x, y] = (
                    max(0, min(255, 92 + haze - int(vertical * 31))),
                    max(0, min(255, 98 + haze - int(vertical * 32))),
                    max(0, min(255, 101 + haze - int(vertical * 32))),
                    246,
                )
            else:
                pixels[x, y] = (
                    max(0, min(255, 246 + haze - int(vertical * 10))),
                    max(0, min(255, 245 + haze - int(vertical * 10))),
                    max(0, min(255, 233 + haze - int(vertical * 8))),
                    238,
                )
    material = add_fine_grain(material, 2, 0xFA02)
    panel = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    inset = 12 * aa
    shadow_mask = Image.new("L", (width, height), 0)
    ImageDraw.Draw(shadow_mask).rounded_rectangle(
        (inset, inset + 3 * aa,
         width - inset - aa, height - inset + 2 * aa),
        radius=24 * aa,
        fill=125,
    )
    shadow_mask = shadow_mask.filter(
        ImageFilter.GaussianBlur(radius=5 * aa)
    )
    shadow = Image.new("RGBA", (width, height), (57, 57, 53, 0))
    shadow.putalpha(shadow_mask)
    panel.alpha_composite(shadow)
    mask = Image.new("L", (width, height), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (inset, inset, width - inset - aa, height - inset - aa),
        radius=24 * aa,
        fill=246,
    )
    panel.paste(material, (0, 0), mask)
    draw = ImageDraw.Draw(panel)
    draw.rounded_rectangle(
        (inset, inset, width - inset - aa, height - inset - aa),
        radius=24 * aa,
        outline=(57, 62, 64, 215) if dark else (143, 143, 134, 190),
        width=2 * aa,
    )
    if dark:
        draw.rounded_rectangle(
            (inset + 2 * aa, inset + 2 * aa,
             width - inset - 3 * aa, height - inset - 3 * aa),
            radius=20 * aa,
            outline=(186, 193, 192, 48),
            width=aa,
        )
    return panel.resize(target_size, Image.Resampling.LANCZOS)


def dot_sprite(size: int, color: tuple[int, int, int, int]) -> Image.Image:
    aa = SUPERSAMPLE
    sprite = Image.new(
        "RGBA", (size * aa, size * aa),
        (color[0], color[1], color[2], 0),
    )
    margin = aa
    ImageDraw.Draw(sprite).ellipse(
        (margin, margin, size * aa - margin - 1, size * aa - margin - 1),
        fill=color,
    )
    return sprite.resize((size, size), Image.Resampling.LANCZOS)


def cover_placeholder(size: int) -> Image.Image:
    """A neutral, rounded missing-artwork tile with a quiet music glyph."""
    aa = SUPERSAMPLE
    scaled = size * aa
    tile = Image.new("RGBA", (scaled, scaled), (0, 0, 0, 0))
    radius = 14 * aa

    fill = Image.new("RGBA", (scaled, scaled), (0, 0, 0, 0))
    fill_pixels = fill.load()
    for y in range(scaled):
        ratio = y / max(1, scaled - 1)
        value = int(225 * (1.0 - ratio) + 207 * ratio)
        for x in range(scaled):
            fill_pixels[x, y] = (value, value + 1, value + 1, 255)
    mask = Image.new("L", (scaled, scaled), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, scaled - 1, scaled - 1),
        radius=radius,
        fill=255,
    )
    tile.paste(fill, (0, 0), mask)

    draw = ImageDraw.Draw(tile)
    draw.rounded_rectangle(
        (aa // 2, aa // 2, scaled - aa // 2 - 1, scaled - aa // 2 - 1),
        radius=radius,
        outline=(132, 143, 145, 220),
        width=aa,
    )

    # Geometric eighth note with its optical center slightly left of the
    # mathematical center, compensating for the right-hand stem and flag.
    note = (113, 132, 137, 242)
    stem_x = 32 * aa
    draw.rounded_rectangle(
        (stem_x, 16 * aa, stem_x + 4 * aa, 42 * aa),
        radius=2 * aa, fill=note,
    )
    draw.polygon(
        ((stem_x + 3 * aa, 16 * aa),
         (42 * aa, 20 * aa),
         (42 * aa, 25 * aa),
         (stem_x + 3 * aa, 21 * aa)),
        fill=note,
    )
    draw.ellipse((18 * aa, 37 * aa, 35 * aa, 48 * aa), fill=note)
    return tile.resize((size, size), Image.Resampling.LANCZOS)


def footer_icon(kind: str, size: int = 32) -> Image.Image:
    """A beveled monochrome glyph intended for runtime accent tinting."""
    aa = SUPERSAMPLE
    scaled = size * aa
    glyph = Image.new("RGBA", (scaled, scaled), (0, 0, 0, 0))
    draw = ImageDraw.Draw(glyph)
    edge = (68, 78, 80, 255)
    fill = (226, 233, 231, 255)
    highlight = (255, 255, 255, 188)

    def point(x: float, y: float) -> tuple[int, int]:
        return (round(x * aa), round(y * aa))

    if kind == "speaker":
        draw.rounded_rectangle(
            (4 * aa, 12 * aa, 10 * aa, 20 * aa),
            radius=2 * aa, fill=fill, outline=edge, width=aa,
        )
        cone = [point(9, 12), point(17, 7), point(17, 25), point(9, 20)]
        draw.polygon(cone, fill=fill)
        draw.line(cone + [cone[0]], fill=edge, width=aa, joint="curve")
        draw.arc(
            (13 * aa, 9 * aa, 27 * aa, 23 * aa),
            start=-55, end=55,
            fill=(247, 251, 249, 255), width=2 * aa,
        )
        draw.arc(
            (14 * aa, 11 * aa, 23 * aa, 21 * aa),
            start=-55, end=55,
            fill=(255, 255, 255, 255), width=aa,
        )
        draw.line(
            (point(10, 12), point(16, 8)),
            fill=highlight, width=aa,
        )
    elif kind == "search":
        draw.ellipse(
            (4 * aa, 4 * aa, 22 * aa, 22 * aa),
            fill=fill, outline=edge, width=2 * aa,
        )
        draw.ellipse(
            (9 * aa, 9 * aa, 17 * aa, 17 * aa),
            fill=(0, 0, 0, 0),
        )
        draw.line(
            (point(19, 19), point(28, 28)),
            fill=edge, width=5 * aa,
        )
        draw.line(
            (point(19, 18), point(27, 26)),
            fill=fill, width=2 * aa,
        )
        draw.arc(
            (6 * aa, 6 * aa, 20 * aa, 20 * aa),
            start=195, end=300, fill=highlight, width=aa,
        )
    elif kind == "gear":
        center = 16.0
        points: list[tuple[int, int]] = []
        for index in range(24):
            angle = -math.pi / 2.0 + index * math.pi / 12.0
            radius = 12.0 if index % 3 == 1 else 9.2
            points.append(point(
                center + math.cos(angle) * radius,
                center + math.sin(angle) * radius,
            ))
        draw.polygon(points, fill=fill)
        draw.line(points + [points[0]], fill=edge, width=aa, joint="curve")
        draw.ellipse(
            (12 * aa, 12 * aa, 20 * aa, 20 * aa),
            fill=(0, 0, 0, 0), outline=edge, width=aa,
        )
        draw.arc(
            (7 * aa, 7 * aa, 25 * aa, 25 * aa),
            start=190, end=300, fill=highlight, width=aa,
        )
    else:
        raise ValueError(f"unknown footer icon: {kind}")

    alpha = glyph.getchannel("A")
    shadow_alpha = Image.new("L", (scaled, scaled), 0)
    shadow_alpha.paste(alpha, (1 * aa, 2 * aa))
    shadow_alpha = shadow_alpha.filter(
        ImageFilter.GaussianBlur(radius=1.2 * aa)
    )
    shadow = Image.new("RGBA", (scaled, scaled), (25, 35, 37, 0))
    shadow.putalpha(shadow_alpha.point(lambda value: value * 150 // 255))
    icon = Image.new("RGBA", (scaled, scaled), (0, 0, 0, 0))
    icon.alpha_composite(shadow)
    icon.alpha_composite(glyph)
    return icon.resize((size, size), Image.Resampling.LANCZOS)


def shoulder_key(label: str, width: int = 40, height: int = 24) -> Image.Image:
    """A compact 3DS-style shoulder key with a geometrically centered glyph."""
    aa = SUPERSAMPLE
    scaled_width = width * aa
    scaled_height = height * aa
    key = Image.new(
        "RGBA", (scaled_width, scaled_height), (0, 0, 0, 0)
    )
    mask = Image.new("L", (scaled_width, scaled_height), 0)
    bounds = (2 * aa, 2 * aa, scaled_width - 2 * aa, scaled_height - 4 * aa)
    ImageDraw.Draw(mask).rounded_rectangle(
        bounds, radius=7 * aa, fill=255
    )

    shadow_mask = Image.new("L", (scaled_width, scaled_height), 0)
    ImageDraw.Draw(shadow_mask).rounded_rectangle(
        (2 * aa, 4 * aa, scaled_width - 2 * aa, scaled_height - aa),
        radius=7 * aa, fill=185,
    )
    shadow_mask = shadow_mask.filter(
        ImageFilter.GaussianBlur(radius=1.5 * aa)
    )
    shadow = Image.new(
        "RGBA", (scaled_width, scaled_height), (38, 45, 45, 0)
    )
    shadow.putalpha(shadow_mask)
    key.alpha_composite(shadow)

    material = Image.new("RGBA", (scaled_width, scaled_height))
    pixels = material.load()
    for y in range(scaled_height):
        ratio = y / max(1, scaled_height - 1)
        value = int(249 * (1.0 - ratio) + 185 * ratio)
        for x in range(scaled_width):
            pixels[x, y] = (value, value + 2, value + 1, 255)
    key.paste(material, (0, 0), mask)

    draw = ImageDraw.Draw(key)
    draw.rounded_rectangle(
        bounds, radius=7 * aa,
        outline=(78, 86, 85, 245), width=aa,
    )
    ink = (54, 62, 62, 255)
    center_x = 20 * aa
    if label == "L":
        draw.line(
            ((center_x - 2 * aa, 7 * aa),
             (center_x - 2 * aa, 15 * aa),
             (center_x + 3 * aa, 15 * aa)),
            fill=ink, width=2 * aa, joint="curve",
        )
    elif label == "R":
        draw.line(
            ((center_x - 4 * aa, 15 * aa),
             (center_x - 4 * aa, 7 * aa),
             (center_x + aa, 7 * aa),
             (center_x + 3 * aa, 9 * aa),
             (center_x + aa, 11 * aa),
             (center_x - 4 * aa, 11 * aa)),
            fill=ink, width=2 * aa, joint="curve",
        )
        draw.line(
            ((center_x, 11 * aa), (center_x + 4 * aa, 15 * aa)),
            fill=ink, width=2 * aa,
        )
    else:
        raise ValueError(f"unknown shoulder key: {label}")
    return key.resize((width, height), Image.Resampling.LANCZOS)


def shoulder_glow(width: int = 40, height: int = 24) -> Image.Image:
    """Exact shoulder-key silhouette used for the transient yellow light."""
    aa = SUPERSAMPLE
    w = width * aa
    h = height * aa
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    fill = Image.new("RGBA", (w, h), (255, 225, 106, 220))
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (2 * aa, 2 * aa, w - 2 * aa, h - 4 * aa),
        radius=7 * aa, fill=255,
    )
    layer.paste(fill, (0, 0), mask)
    return layer.resize((width, height), Image.Resampling.LANCZOS)


def face_key(label: str, size: int = 16) -> Image.Image:
    """Nintendo-style glossy face-button badge for compact control hints."""
    aa = SUPERSAMPLE
    scaled = size * aa
    key = Image.new("RGBA", (scaled, scaled), (0, 0, 0, 0))
    draw = ImageDraw.Draw(key)
    draw.ellipse(
        (aa, 2 * aa, scaled - 2 * aa, scaled - aa),
        fill=(47, 55, 56, 118),
    )
    draw.ellipse(
        (aa, aa, scaled - 2 * aa, scaled - 2 * aa),
        fill=(229, 234, 231, 255),
        outline=(103, 113, 112, 255),
        width=aa,
    )
    draw.arc(
        (2 * aa, 2 * aa, scaled - 3 * aa, scaled - 3 * aa),
        start=205, end=335, fill=(255, 255, 255, 245), width=aa,
    )
    if label not in ("A", "B", "X", "Y"):
        raise ValueError(f"unknown face key: {label}")
    ink = (66, 80, 83, 255)
    try:
        font = ImageFont.truetype(
            "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
            8 * aa,
        )
    except OSError:
        font = ImageFont.truetype("DejaVuSans-Bold.ttf", 8 * aa)
    bounds = draw.textbbox((0, 0), label, font=font)
    text_width = bounds[2] - bounds[0]
    text_height = bounds[3] - bounds[1]
    draw.text(
        ((scaled - text_width) / 2 - bounds[0],
         (scaled - text_height) / 2 - bounds[1] - aa * 0.25),
        label, font=font, fill=ink,
    )
    return key.resize((size, size), Image.Resampling.LANCZOS)


def dpad_key(size: int = 16) -> Image.Image:
    """Nintendo-style D-pad glyph used by every control-hint surface."""
    aa = SUPERSAMPLE
    scaled = size * aa
    key = Image.new("RGBA", (scaled, scaled), (0, 0, 0, 0))
    draw = ImageDraw.Draw(key)
    ink = (76, 100, 108, 255)
    draw.rounded_rectangle(
        (6 * aa, aa, 10 * aa, 15 * aa),
        radius=aa, fill=ink,
    )
    draw.rounded_rectangle(
        (aa, 6 * aa, 15 * aa, 10 * aa),
        radius=aa, fill=ink,
    )
    return key.resize((size, size), Image.Resampling.LANCZOS)


def select_key(width: int = 32, height: int = 16) -> Image.Image:
    """Small silver SELECT keycap matching the face and shoulder buttons."""
    aa = SUPERSAMPLE
    w = width * aa
    h = height * aa
    key = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(key)
    draw.rounded_rectangle(
        (aa, 2 * aa, w - aa, h - aa),
        radius=5 * aa, fill=(47, 55, 56, 118),
    )
    draw.rounded_rectangle(
        (aa, aa, w - aa, h - 2 * aa),
        radius=5 * aa, fill=(229, 234, 231, 255),
        outline=(103, 113, 112, 255), width=aa,
    )
    draw.arc(
        (2 * aa, 2 * aa, w - 2 * aa, h - 3 * aa),
        start=205, end=335, fill=(255, 255, 255, 245), width=aa,
    )
    ink = (66, 80, 83, 255)
    try:
        font = ImageFont.truetype(
            "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
            5 * aa,
        )
    except OSError:
        font = ImageFont.truetype("DejaVuSans-Bold.ttf", 5 * aa)
    label = "SELECT"
    bounds = draw.textbbox((0, 0), label, font=font)
    text_width = bounds[2] - bounds[0]
    text_height = bounds[3] - bounds[1]
    draw.text(
        ((w - text_width) / 2 - bounds[0],
         (h - text_height) / 2 - bounds[1] - aa * 0.25),
        label, font=font, fill=ink,
    )
    return key.resize((width, height), Image.Resampling.LANCZOS)


def hint_strip(kind: str) -> Image.Image:
    """Pre-rendered official-style keycaps; action labels stay real text."""
    width = 312 if kind == "ime" else 88
    height = 14
    aa = SUPERSAMPLE
    strip = Image.new("RGBA", (width * aa, height * aa), (0, 0, 0, 0))
    draw = ImageDraw.Draw(strip)
    ink = (76, 100, 108, 255)

    def paste_key(label: str, center_x: int) -> None:
        key = face_key(label, height).resize(
            (height * aa, height * aa), Image.Resampling.NEAREST
        )
        strip.alpha_composite(key, ((center_x - height // 2) * aa, 0))

    if kind == "search":
        paste_key("X", 8)
        paste_key("B", 62)
    elif kind == "ime":
        # Nintendo-style D-pad plus the same silver face-key material.
        draw.rounded_rectangle(
            (5 * aa, aa, 9 * aa, 13 * aa),
            radius=aa, fill=ink,
        )
        draw.rounded_rectangle(
            (aa, 5 * aa, 13 * aa, 9 * aa),
            radius=aa, fill=ink,
        )
        paste_key("A", 88)
        paste_key("Y", 139)
        paste_key("B", 220)
    else:
        raise ValueError(f"unknown hint strip: {kind}")
    return strip.resize((width, height), Image.Resampling.LANCZOS)


def player_option_icon(kind: str, size: int = 16) -> Image.Image:
    """Compact, mainstream music-player symbols drawn into the skin atlas."""
    aa = SUPERSAMPLE
    icon = Image.new("RGBA", (size * aa, size * aa), (0, 0, 0, 0))
    draw = ImageDraw.Draw(icon)
    ink = (255, 255, 255, 255)
    stroke = 2 * aa

    def line(points: list[tuple[float, float]], width: int = stroke) -> None:
        draw.line(
            [(round(x * aa), round(y * aa)) for x, y in points],
            fill=ink, width=width, joint="curve",
        )

    def arrow_head(x: float, y: float, direction: int) -> None:
        draw.polygon(
            [
                (round((x - 2.5 * direction) * aa),
                 round((y - 2.5) * aa)),
                (round((x - 2.5 * direction) * aa),
                 round((y + 2.5) * aa)),
                (round(x * aa), round(y * aa)),
            ],
            fill=ink,
        )

    if kind == "sequence":
        # A left-to-right timeline reads as normal sequential playback.
        line([(2.0, 8.0), (13.5, 8.0)], width=aa)
        for x in (3.0, 7.0, 11.0):
            draw.ellipse(
                ((x - 1.2) * aa, 6.8 * aa,
                 (x + 1.2) * aa, 9.2 * aa),
                fill=ink,
            )
        arrow_head(14.5, 8.0, 1)
    elif kind == "repeat_one":
        # A circular replay arrow leaves enough room for a legible “1”.
        draw.arc(
            (2 * aa, 2 * aa, 14 * aa, 14 * aa),
            start=35, end=330, fill=ink, width=stroke,
        )
        draw.polygon(
            ((12.0 * aa, 2.0 * aa),
             (15.0 * aa, 3.0 * aa),
             (13.0 * aa, 5.5 * aa)),
            fill=ink,
        )
        line([(7.0, 7.0), (8.2, 6.0), (8.2, 11.0)])
    elif kind == "shuffle":
        line([(2.0, 4.0), (4.5, 4.0), (11.5, 11.0), (14.0, 11.0)])
        line([(2.0, 12.0), (4.5, 12.0), (11.5, 5.0), (14.0, 5.0)])
        arrow_head(14.0, 5.0, 1)
        arrow_head(14.0, 11.0, 1)
    elif kind == "scope":
        # Smooth oscilloscope curve, not the old jagged ECG-like polyline.
        points = []
        for index in range(29):
            x = 1.0 + index * 0.5
            y = 8.0 - math.sin(index / 28.0 * math.tau * 1.5) * 4.0
            points.append((x, y))
        line(points, width=aa)
    elif kind == "spectrum":
        heights = (5.0, 10.0, 14.0, 8.0, 4.0)
        for index, height in enumerate(heights):
            x = (1.0 + index * 3.0) * aa
            draw.rounded_rectangle(
                (x, round((15.0 - height) * aa),
                 x + 2 * aa, 15 * aa),
                radius=aa, fill=ink,
            )
    elif kind == "levels":
        for row, active in enumerate((4, 3)):
            y = (3 + row * 7) * aa
            for column in range(4):
                alpha = 255 if column < active else 105
                draw.rounded_rectangle(
                    ((1 + column * 4) * aa, y,
                     (3 + column * 4) * aa, y + 3 * aa),
                    radius=aa, fill=(255, 255, 255, alpha),
                )
    elif kind == "none":
        heights = (5.0, 10.0, 14.0, 8.0, 4.0)
        for index, height in enumerate(heights):
            x = (1.0 + index * 3.0) * aa
            draw.rounded_rectangle(
                (x, round((15.0 - height) * aa),
                 x + 2 * aa, 15 * aa),
                radius=aa, fill=ink,
            )
        line([(2.0, 2.0), (14.0, 14.0)])
    else:
        raise ValueError(f"unknown player option icon: {kind}")
    return icon.resize((size, size), Image.Resampling.LANCZOS)


def footer_bar(width: int = 112, height: int = 64) -> Image.Image:
    """System Settings-style bottom sheet with shallow top corners."""
    aa = SUPERSAMPLE
    w = width * aa
    h = height * aa
    bar = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    mask = Image.new("L", (w, h), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.rounded_rectangle(
        (0, 0, w - 1, h + 32 * aa),
        radius=28 * aa, fill=255,
    )

    material = Image.new("RGBA", (w, h))
    pixels = material.load()
    for y in range(h):
        ratio = y / max(1, h - 1)
        if ratio < 0.22:
            local = ratio / 0.22
            value = int(126 * (1.0 - local) + 86 * local)
        else:
            local = (ratio - 0.22) / 0.78
            value = int(86 * (1.0 - local) + 43 * local)
        for x in range(w):
            pixels[x, y] = (value, value + 7, value + 8, 252)
    bar.paste(material, (0, 0), mask)

    draw = ImageDraw.Draw(bar)
    draw.line(
        (28 * aa, 2 * aa, w - 28 * aa, 2 * aa),
        fill=(225, 230, 226, 185), width=aa,
    )
    draw.line(
        (28 * aa, 4 * aa, w - 28 * aa, 4 * aa),
        fill=(137, 147, 148, 180), width=aa,
    )
    draw.rounded_rectangle(
        (0, 0, w - 1, h + 32 * aa),
        radius=28 * aa, outline=(48, 55, 57, 245), width=aa,
    )
    return bar.resize((width, height), Image.Resampling.LANCZOS)


def battery_icon(level: int, width: int = 24,
                 height: int = 16) -> Image.Image:
    """Glossy segmented battery matching the Nintendo system UI language."""
    aa = SUPERSAMPLE
    w = width * aa
    h = height * aa
    icon = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(icon)
    body = (2 * aa, 2 * aa, 19 * aa, 14 * aa)
    draw.rounded_rectangle(
        (3 * aa, 3 * aa, 20 * aa, 15 * aa),
        radius=3 * aa, fill=(20, 27, 28, 145),
    )
    draw.rounded_rectangle(
        body, radius=3 * aa,
        fill=(238, 241, 235, 255),
        outline=(75, 83, 82, 255), width=aa,
    )
    draw.rounded_rectangle(
        (4 * aa, 4 * aa, 17 * aa, 12 * aa),
        radius=2 * aa, fill=(103, 112, 112, 255),
    )
    draw.rounded_rectangle(
        (19 * aa, 6 * aa, 22 * aa, 11 * aa),
        radius=aa, fill=(185, 191, 187, 255),
        outline=(75, 83, 82, 255), width=aa,
    )
    segment = (58, 164, 201, 255)
    for index in range(max(0, min(3, level))):
        x0 = (5 + index * 4) * aa
        draw.rounded_rectangle(
            (x0, 5 * aa, x0 + 3 * aa, 11 * aa),
            radius=aa, fill=segment,
        )
        draw.line(
            (x0 + aa, 5 * aa, x0 + 2 * aa, 5 * aa),
            fill=(164, 226, 242, 230), width=aa,
        )
    return icon.resize((width, height), Image.Resampling.LANCZOS)


def charging_icon(size: int = 16) -> Image.Image:
    aa = SUPERSAMPLE
    icon = Image.new("RGBA", (size * aa, size * aa), (0, 0, 0, 0))
    draw = ImageDraw.Draw(icon)
    points = (
        (9 * aa, aa), (4 * aa, 8 * aa), (8 * aa, 8 * aa),
        (5 * aa, 15 * aa), (13 * aa, 6 * aa), (9 * aa, 6 * aa),
    )
    draw.polygon(points, fill=(255, 184, 56, 255))
    draw.line(
        points + (points[0],),
        fill=(181, 122, 26, 255), width=aa, joint="curve",
    )
    draw.line(
        (8 * aa, 3 * aa, 6 * aa, 7 * aa),
        fill=(255, 237, 166, 220), width=aa,
    )
    return icon.resize((size, size), Image.Resampling.LANCZOS)


def build_atlas(output: Path, dark: bool = False) -> None:
    atlas = Image.new("RGBA", (ATLAS_SIZE, ATLAS_SIZE), (0, 0, 0, 0))

    panel = frosted_panel(512, 256, dark=dark)
    atlas.paste(panel, (0, 0), panel)

    normal_button = glossy_tile(
        192, 80,
        (126, 131, 132) if dark else (255, 255, 252),
        (46, 50, 52) if dark else (207, 205, 195),
        (44, 48, 50, 232) if dark else (111, 112, 107, 238),
        22, 0xFA03,
        shadow_alpha=205,
        shadow_offset=4,
        shadow_blur=2.2,
        inner_highlight_alpha=52 if dark else 160,
        top_line_alpha=78 if dark else 196,
        bottom_line_alpha=178 if dark else 126,
    )
    active_button = glossy_tile(
        192, 80,
        (138, 143, 144) if dark else (255, 255, 253),
        (55, 59, 61) if dark else (190, 194, 191),
        (45, 49, 51, 236) if dark else (104, 108, 106, 245),
        22, 0xFA04,
        shadow_alpha=215,
        shadow_offset=4,
        shadow_blur=2.0,
        inner_highlight_alpha=62 if dark else 174,
        top_line_alpha=92 if dark else 215,
        bottom_line_alpha=190 if dark else 138,
    )
    atlas.paste(normal_button, (0, 272), normal_button)
    atlas.paste(active_button, (208, 272), active_button)

    selection = glossy_tile(
        256, 64, (255, 255, 242), (226, 205, 116),
        (151, 132, 69, 235), 16, 0xFA05,
        shadow_alpha=215, shadow_offset=3, shadow_blur=2.0,
        inner_highlight_alpha=176, top_line_alpha=210,
        bottom_line_alpha=126,
    )
    atlas.paste(selection, (0, 368), selection)

    header = glossy_tile(
        256, 64, (105, 111, 112), (43, 50, 53),
        (239, 191, 82, 230), 12, 0xFA06
    )
    atlas.paste(header, (256, 368), header)

    progress = glossy_tile(
        256, 32, (138, 143, 142), (65, 72, 73),
        (245, 231, 188, 210), 16, 0xFA07
    )
    atlas.paste(progress, (0, 464), progress)

    pressed_button = button_glow_layer(128, 64)
    atlas.paste(pressed_button, (272, 448), pressed_button)

    dot_colors = (
        (13, 170, 193, 255),
        (250, 255, 252, 255),
        (255, 184, 70, 255),
        (89, 184, 94, 255),
        (132, 158, 169, 255),
        (255, 83, 105, 255),
        (244, 249, 248, 238),
        (242, 159, 42, 255),
        (93, 177, 163, 255),
        (58, 164, 201, 255),
        (224, 112, 141, 255),
        (88, 164, 188, 255),
        (55, 174, 142, 255),
    )
    for index, color in enumerate(dot_colors):
        x = 400 + (index if index < 7 else index - 7) * 16
        y = 272 if index < 7 else 288
        dot = dot_sprite(16, color)
        atlas.paste(dot, (x, y), dot)

    placeholder = cover_placeholder(64)
    atlas.paste(placeholder, (400, 304), placeholder)
    ime_hints = hint_strip("ime")
    search_hints = hint_strip("search")
    atlas.paste(ime_hints, (0, 353), ime_hints)
    atlas.paste(search_hints, (312, 353), search_hints)
    for index, kind in enumerate((
        "sequence", "repeat_one", "shuffle",
        "scope", "spectrum", "levels", "none",
    )):
        option_icon = player_option_icon(kind)
        atlas.paste(option_icon, (index * 16, 256), option_icon)

    # The System Settings footer is a shallow bottom sheet rather than a pill.
    footer = footer_bar()
    atlas.paste(footer, (400, 448), footer)

    for index, kind in enumerate(("speaker", "search", "gear")):
        icon = footer_icon(kind)
        atlas.paste(icon, (index * 32, 432), icon)
    left_key = shoulder_key("L")
    right_key = shoulder_key("R")
    atlas.paste(left_key, (96, 432), left_key)
    atlas.paste(right_key, (136, 432), right_key)
    for level in range(4):
        battery = battery_icon(level)
        atlas.paste(battery, (176 + level * 24, 432), battery)
    charging = charging_icon()
    atlas.paste(charging, (272, 432), charging)
    atlas.paste(dpad_key(), (288, 432), dpad_key())
    for index, label in enumerate(("A", "B", "X", "Y")):
        key = face_key(label)
        atlas.paste(key, (304 + index * 16, 432), key)
    select = select_key()
    atlas.paste(select, (368, 432), select)
    cover_inset = cover_inset_tile(dark)
    atlas.paste(cover_inset, (464, 328), cover_inset)
    key_glow = shoulder_glow()
    atlas.paste(key_glow, (464, 304), key_glow)

    output.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(output, optimize=True)


def main() -> None:
    global SUPERSAMPLE
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dark", action="store_true")
    args = parser.parse_args()
    if args.dark:
        # Dark rims expose pixel stair-steps much more readily than the pale
        # material, so render this variant at twice the linear resolution.
        SUPERSAMPLE = 8
    build_atlas(args.output, dark=args.dark)


if __name__ == "__main__":
    main()
