#!/usr/bin/env python3
"""Regression tests for Japanese glyph-foot alignment."""

import importlib.util
from pathlib import Path
import sys
import types
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
try:
    import fontTools.ttLib  # noqa: F401
except ModuleNotFoundError:
    font_tools = types.ModuleType("fontTools")
    tt_lib = types.ModuleType("fontTools.ttLib")
    tt_lib.TTFont = object
    font_tools.ttLib = tt_lib
    sys.modules["fontTools"] = font_tools
    sys.modules["fontTools.ttLib"] = tt_lib
SPEC = importlib.util.spec_from_file_location(
    "gen_immersive_font", ROOT / "tools" / "gen_immersive_font.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class JapaneseGlyphFootAlignmentTests(unittest.TestCase):
    def test_baseline_glyph_is_aligned(self):
        image = Image.new("L", (24, 32), 0)
        for y in range(5, 27):
            image.putpixel((8, y), 255)
        aligned = MODULE.align_japanese_visible_bottom(
            image, ord("詞"), 27
        )
        self.assertEqual(aligned.getbbox(), (8, 6, 9, 28))

    def test_floating_kana_is_preserved(self):
        image = Image.new("L", (24, 32), 0)
        for y in range(8, 25):
            image.putpixel((8, y), 255)
        aligned = MODULE.align_japanese_visible_bottom(
            image, ord("べ"), 27
        )
        self.assertEqual(aligned.getbbox(), image.getbbox())

    def test_baseline_near_kana_is_aligned(self):
        image = Image.new("L", (24, 32), 0)
        for y in range(6, 27):
            image.putpixel((8, y), 255)
        aligned = MODULE.align_japanese_visible_bottom(
            image, ord("な"), 27
        )
        self.assertEqual(aligned.getbbox(), (8, 7, 9, 28))

    def test_high_full_size_kana_is_aligned(self):
        image = Image.new("L", (24, 32), 0)
        for y in range(8, 26):
            image.putpixel((8, y), 255)
        aligned = MODULE.align_japanese_visible_bottom(
            image, ord("ハ"), 27
        )
        self.assertEqual(aligned.getbbox(), (8, 10, 9, 28))

    def test_small_kana_is_preserved(self):
        image = Image.new("L", (24, 32), 0)
        for y in range(7, 27):
            image.putpixel((8, y), 255)
        aligned = MODULE.align_japanese_visible_bottom(
            image, ord("ぁ"), 27
        )
        self.assertEqual(aligned.getbbox(), image.getbbox())

    def test_midline_han_is_preserved(self):
        image = Image.new("L", (24, 32), 0)
        for x in range(2, 22):
            image.putpixel((x, 16), 255)
        aligned = MODULE.align_japanese_visible_bottom(
            image, ord("一"), 27
        )
        self.assertEqual(aligned.getbbox(), image.getbbox())

    def test_latin_letters_share_one_baseline_shift(self):
        image = Image.new("L", (24, 32), 0)
        for y in range(6, 26):
            image.putpixel((8, y), 255)
        shifted = MODULE.shift_alphabetic_baseline(image, ord("K"), 1)
        self.assertEqual(shifted.getbbox(), (8, 7, 9, 27))

    def test_greek_and_cyrillic_use_the_alphabetic_baseline(self):
        for char in ("Ω", "Ж"):
            with self.subTest(char=char):
                self.assertTrue(
                    MODULE.is_alphabetic_baseline_glyph(ord(char)))

    def test_cjk_scripts_keep_the_font_native_baseline(self):
        for char in ("词", "な", "한"):
            with self.subTest(char=char):
                image = Image.new("L", (24, 32), 0)
                image.putpixel((8, 25), 255)
                shifted = MODULE.shift_alphabetic_baseline(
                    image, ord(char), 1)
                self.assertEqual(shifted.getbbox(), image.getbbox())

    def test_negative_left_bearing_is_kept_inside_the_cell(self):
        class Font:
            @staticmethod
            def getbbox(_char, anchor):
                self.assertEqual(anchor, "ls")
                return (-2, -12, 6, 4)

        self.assertEqual(MODULE.glyph_origin_x(Font(), "j"), 2)

    def test_nonnegative_left_bearing_keeps_the_native_origin(self):
        class Font:
            @staticmethod
            def getbbox(_char, anchor):
                self.assertEqual(anchor, "ls")
                return (1, -12, 9, 0)

        self.assertEqual(MODULE.glyph_origin_x(Font(), "K"), 0)


if __name__ == "__main__":
    unittest.main()
