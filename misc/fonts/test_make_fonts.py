# Copyright (C) 2026 the OpenMOHAA Plus contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Unit tests for make_fonts.py (need Pillow and the DejaVu fonts; skipped otherwise)."""
import os
import re
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import make_fonts
except ImportError:  # Pillow missing
    make_fonts = None

TTF = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


@unittest.skipUnless(make_fonts and os.path.exists(TTF), "needs Pillow and DejaVu fonts")
class MakeFonts(unittest.TestCase):
    def build(self, reference=None, scale=2):
        text, tga, count = make_fonts.build("verdana-14", 14, TTF, scale, reference)
        indirection = [int(v) for v in re.search(r"indirections\s*\{([^}]*)\}", text).group(1).split()]
        locations = re.findall(r"\{\s*([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*\}", text.split("locations", 1)[1])
        return text, tga, count, indirection, [tuple(map(float, loc)) for loc in locations]

    def test_layout_the_game_reads(self):
        text, tga, count, indirection, locations = self.build()
        self.assertTrue(text.startswith("RitFont\r\nheight 14.000000\r\naspect "))
        self.assertEqual(len(indirection), 256)
        self.assertEqual(len(locations), 256)
        for ch in "Aa0ñÑáéíóú¿¡çü€":
            index = indirection[ch.encode("cp1252")[0]]
            self.assertGreaterEqual(index, 0, ch)
            self.assertEqual(locations[index][3], 14.0)       # every cell is the font height
        self.assertEqual(indirection[0x0A], -1)               # control characters have no glyph
        self.assertGreater(count, 200)

    def test_tga_is_bottom_up_at_the_requested_scale(self):
        _, tga, _, _, _ = self.build(scale=3)
        id_len, cmap, kind = tga[0], tga[1], tga[2]
        width, height, bpp, descriptor = struct.unpack("<HHBB", tga[12:18])
        self.assertEqual((id_len, cmap, kind, bpp), (0, 0, 2, 32))
        self.assertEqual(width, 256 * 3)
        self.assertEqual(descriptor & 0x20, 0)               # the game ignores the top-down flag
        self.assertEqual(len(tga), 18 + width * height * 4)

    def test_widths_match_the_reference(self):
        _, _, _, indirection, locations = self.build(reference=477.0)
        total = sum(locations[indirection[ord(c)]][2] for c in make_fonts.SAMPLE)
        self.assertAlmostEqual(total, 477.0, delta=477.0 * 0.03)   # rounding per glyph


if __name__ == "__main__":
    unittest.main()
