#!/usr/bin/env python3

import importlib.util
from pathlib import Path
import sys


sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "gen_japanese_font_whitelist",
    ROOT / "tools/gen_japanese_font_whitelist.py",
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def main() -> int:
    values = set(MODULE.codepoints())
    for character in "あいうえおアイウエオ愛夢踊働":
        assert ord(character) in values
    assert 0x3400 in values
    assert 0x9FFF in values
    assert ord("A") not in values
    print("Japanese font whitelist tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

