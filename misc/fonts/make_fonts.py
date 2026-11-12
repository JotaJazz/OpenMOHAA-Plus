#!/usr/bin/env python3
# Copyright (C) 2026 the OpenMOHAA Plus contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Build MOHAA bitmap fonts (fonts/<name>.RitualFont + gfx/fonts/<name>.tga) from TrueType fonts.

The game's fonts only hold printable ASCII at 1x. These keep each font's name, height and layout
rules, add the Latin-1 / Windows-1252 letters (á é í ó ú ñ ¿ ¡ ç ü ...) and draw the texture at
--scale times the nominal size, so text is sharper when the HUD is scaled (ui_hudscale, 4K). The
size on screen does not change: the game reads glyph sizes in nominal pixels.

  python3 misc/fonts/make_fonts.py --out zzz-plus-fonts.pk3            (DejaVu fonts, default set)
  python3 misc/fonts/make_fonts.py --out dir/ --font verdana-14=/path/Verdana.ttf

Requires Pillow (python3 -m pip install pillow). The default set uses the DejaVu fonts
(Bitstream Vera license, DejaVu changes in the public domain); handle-* (ornamental) and marlett
(symbols) are not replaced.
"""
import argparse
import io
import os
import struct
import sys
import zipfile

from PIL import Image, ImageDraw, ImageFilter, ImageFont

# Pillow < 9.1 has no Image.Transpose / Image.Resampling
FLIP_TOP_BOTTOM = getattr(getattr(Image, "Transpose", Image), "FLIP_TOP_BOTTOM")
LANCZOS = getattr(getattr(Image, "Resampling", Image), "LANCZOS")

DEJAVU = "/usr/share/fonts/truetype/dejavu"
# Width of SAMPLE in nominal pixels in the original game fonts, used when --game-dir is not given
# (texts must fit the boxes the menus and HUD were laid out for)
REFERENCE_WIDTHS = {"facfont-20": 484.0, "verdana-12": 405.0, "verdana-14": 477.0,
                    "courier-16": 558.0, "courier-18": 620.0, "courier-20": 682.0}
DEFAULT_SET = {
    "facfont-20": "DejaVuSansCondensed-Bold.ttf",
    "verdana-12": "DejaVuSans.ttf",
    "verdana-14": "DejaVuSans.ttf",
    "courier-16": "DejaVuSansMono-Bold.ttf",
    "courier-18": "DejaVuSansMono-Bold.ttf",
    "courier-20": "DejaVuSansMono-Bold.ttf",
}
# the game's HUD font is light grey with a dark outline (readable on bright walls); the others are white
STYLES = {"facfont-20": {"fill": 219, "outline": True}}
# the game's text is single-byte Windows-1252: printable ASCII and 0x80-0xFF
CODES = list(range(0x20, 0x7F)) + list(range(0x80, 0x100))
ATLAS_WIDTH = 256  # nominal pixels: the game divides glyph positions by 256


def char_of(code):
    try:
        return bytes([code]).decode("cp1252")
    except UnicodeDecodeError:
        return None


def fit_font(path, cell_px):
    """The largest size whose ascent + descent fits the cell height."""
    low, high = 4, cell_px * 2
    while low < high:
        mid = (low + high + 1) // 2
        ascent, descent = ImageFont.truetype(path, mid).getmetrics()
        if ascent + descent <= cell_px:
            low = mid
        else:
            high = mid - 1
    return ImageFont.truetype(path, low)


def render(font, ch):
    image = Image.new("L", (font.size * 3, font.size * 3))
    ImageDraw.Draw(image).text((font.size, font.size), ch, font=font, fill=255)
    return image.tobytes()


def has_glyph(font, ch):
    # a missing glyph renders as .notdef (an empty box or nothing): compare with a surely missing one
    if ch in (" ", chr(0xA0)):
        return True
    return render(font, ch) not in (render(font, chr(0xFFFF)), render(font, " "))


SAMPLE = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"


def build(name, height, ttf, scale, reference_width=None, fill=255, outline=False):
    """Return (ritualfont text, TGA bytes, glyph count).

    reference_width: the original font's nominal width of SAMPLE; glyphs are squeezed or stretched
    horizontally to match it, so texts fit the boxes the game's menus and HUD were laid out for.
    """
    cell = height * scale
    font = fit_font(ttf, cell)
    ascent, descent = font.getmetrics()
    top = (cell - ascent - descent) // 2
    pad = 2 + (2 * scale if outline else 0)  # texels between glyphs: bilinear bleeding, outline

    stretch = 1.0
    if reference_width:
        stretch = reference_width * scale / sum(font.getlength(c) for c in SAMPLE)

    glyphs = []
    for code in CODES:
        ch = char_of(code)
        if ch is None or not has_glyph(font, ch):
            continue
        glyphs.append((code, ch, max(1, round(font.getlength(ch) * stretch))))

    # pack rows of ATLAS_WIDTH * scale texels
    width = ATLAS_WIDTH * scale
    x = y = 0
    placed = []
    for code, ch, advance in glyphs:
        if x + advance > width:
            x, y = 0, y + cell + pad
        placed.append((code, ch, x, y, advance))
        x += advance + pad
    used = y + cell
    tex_height = 16
    while tex_height < used:
        tex_height *= 2

    image = Image.new("RGBA", (width, tex_height), (255, 255, 255, 0))
    draw = ImageDraw.Draw(image)
    for code, ch, gx, gy, advance in placed:
        if stretch == 1.0:
            draw.text((gx, gy + top), ch, font=font, fill=(255, 255, 255, 255))
            continue
        natural = max(1, round(font.getlength(ch)))
        glyph = Image.new("RGBA", (natural + 2 * cell, cell), (255, 255, 255, 0))
        ImageDraw.Draw(glyph).text((cell, top), ch, font=font, fill=(255, 255, 255, 255))
        glyph = glyph.resize((max(1, round(glyph.width * stretch)), cell), LANCZOS)
        offset = round(cell * stretch)
        image.alpha_composite(glyph.crop((offset, 0, offset + advance, cell)), (gx, gy))

    if fill != 255 or outline:
        mask = image.getchannel("A")
        styled = Image.new("RGBA", image.size, (0, 0, 0, 0))
        if outline:
            # one nominal pixel of dark edge around every glyph, like the game's own HUD font
            edge = mask.filter(ImageFilter.MaxFilter(2 * scale + 1)).point(lambda v: v * 0.8)
            styled.putalpha(edge)
        styled.alpha_composite(Image.merge("RGBA", (mask.point(lambda v: fill),) * 3 + (mask,)))
        image = styled

    nominal_height = tex_height / scale
    aspect = ATLAS_WIDTH / nominal_height
    indirection = [-1] * 256
    locations = [(0.0, 0.0, 0.0, 0.0)] * 256
    for index, (code, ch, gx, gy, advance) in enumerate(placed):
        indirection[code] = index
        locations[index] = (gx / scale, gy / scale, advance / scale, height)

    lines = ["RitFont", f"height {height:.6f}", f"aspect {aspect:.8f}", "indirections {"]
    for i in range(0, 256, 10):
        lines.append(" ".join(str(v) for v in indirection[i:i + 10]) + " ")
    lines.append("}")
    lines.append("locations {")
    for i in range(0, 256, 3):
        lines.append(" ".join("{ %.8f %.8f %.8f %.8f }" % loc for loc in locations[i:i + 3]) + " ")
    lines.append("}")
    text = "\r\n".join(lines) + "\r\n"
    return text, tga_bytes(image), len(placed)


def original_widths(game_dir):
    """SAMPLE widths of the game's own fonts (nominal pixels)."""
    import glob
    import re

    widths = {}
    for pak in sorted(glob.glob(os.path.join(game_dir, "main", "*.pk3")), key=str.lower):
        with zipfile.ZipFile(pak) as z:
            for entry in z.namelist():
                low = entry.lower()
                if not (low.startswith("fonts/") and low.endswith(".ritualfont")):
                    continue
                text = z.read(entry).decode("latin-1")
                indirection = [int(v) for v in re.search(r"indirections\s*\{([^}]*)\}", text).group(1).split()]
                locations = re.findall(r"\{\s*[\d.\-]+\s+[\d.\-]+\s+([\d.\-]+)\s+[\d.\-]+\s*\}",
                                       text.split("locations", 1)[1])
                if all(indirection[ord(c)] >= 0 for c in SAMPLE):
                    widths[low[6:-11]] = sum(float(locations[indirection[ord(c)]]) for c in SAMPLE)
    return widths


def tga_bytes(image):
    """Uncompressed 32-bit TGA, bottom row first (the game ignores the top-down flag)."""
    w, h = image.size
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 32, 0x08)
    r, g, b, a = image.transpose(FLIP_TOP_BOTTOM).split()
    return header + Image.merge("RGBA", (b, g, r, a)).tobytes()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True, help="a .pk3 file, or a folder (the game's main/ or a mod folder)")
    ap.add_argument("--font", action="append", default=[], metavar="NAME=TTF",
                    help="build NAME (e.g. verdana-14) from this TrueType file; replaces the default set")
    ap.add_argument("--scale", type=int, default=2, help="texture resolution multiplier (default 2)")
    ap.add_argument("--game-dir", help="measure the widths of the original fonts in <GAME_DIR>/main/*.pk3")
    args = ap.parse_args(argv)

    fonts = {}
    if args.font:
        for item in args.font:
            name, _, path = item.partition("=")
            fonts[name] = path
    else:
        fonts = {name: os.path.join(DEJAVU, ttf) for name, ttf in DEFAULT_SET.items()}

    references = dict(REFERENCE_WIDTHS)
    if args.game_dir:
        references.update(original_widths(args.game_dir))

    files = {}
    for name, path in fonts.items():
        try:
            height = int(name.rsplit("-", 1)[1])
        except (IndexError, ValueError):
            ap.error(f"{name}: font names end with their height, e.g. verdana-14")
        if not os.path.exists(path):
            ap.error(f"{name}: {path} not found")
        style = STYLES.get(name, {}) if not args.font else {}
        text, tga, count = build(name, height, path, args.scale, references.get(name), **style)
        files[f"fonts/{name}.RitualFont"] = text.encode("ascii")
        files[f"gfx/fonts/{name}.tga"] = tga
        print(f"{name}: {count} glyphs from {os.path.basename(path)}")

    if not args.font:
        # the Bitstream Vera license requires its notice with derived fonts
        notice = "/usr/share/doc/fonts-dejavu-core/copyright"
        text = open(notice, encoding="utf-8").read() if os.path.exists(notice) else ""
        files["fonts/plus-fonts-license.txt"] = (
            "Bitmap fonts made by OpenMOHAA Plus misc/fonts/make_fonts.py from the DejaVu fonts\n"
            "(https://dejavu-fonts.github.io/). DejaVu changes are in the public domain; the Bitstream Vera\n"
            "glyphs are under the Bitstream Vera license:\n\n" + text).encode("utf-8")

    if args.out.lower().endswith(".pk3"):
        with zipfile.ZipFile(args.out, "w", zipfile.ZIP_DEFLATED) as z:
            for name, data in files.items():
                z.writestr(name, data)
    else:
        for name, data in files.items():
            path = os.path.join(args.out, name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as f:
                f.write(data)
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
