#!/usr/bin/env python3
"""Generate the Japanese lyric coverage used by the point-font atlases."""

from __future__ import annotations

import argparse
from pathlib import Path


RANGES = (
    (0x3000, 0x303F),  # CJK punctuation
    (0x3040, 0x309F),  # Hiragana
    (0x30A0, 0x30FF),  # Katakana
    (0x31F0, 0x31FF),  # Katakana phonetic extensions
    (0x3400, 0x4DBF),  # CJK Extension A (names and uncommon lyrics)
    (0x4E00, 0x9FFF),  # BMP unified ideographs, including Japanese forms
    (0xFF61, 0xFF9F),  # Half-width Japanese punctuation and katakana
)


def codepoints() -> list[int]:
    return [
        value
        for first, last in RANGES
        for value in range(first, last + 1)
    ]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    values = codepoints()
    args.output.write_text(
        "".join(f"{value}\n" for value in values), encoding="ascii"
    )
    print(
        f"wrote {args.output}: {len(values):,} Japanese/CJK codepoints"
    )


if __name__ == "__main__":
    main()

