# OpenMOHAA Plus: fonts with accents and sharper text

The game's fonts only contain English letters, drawn at their original small size. OpenMOHAA Plus
can use replacement fonts that add the Latin-1 / Windows-1252 letters (á é í ó ú ñ ¿ ¡ ç ü ß ...)
and are drawn at twice the resolution, so text stays sharp when the HUD is scaled (`ui_hudscale 2`
at 1920x1080, or 4K screens). Sizes and layout do not change: each font keeps the widths of the
original, so texts fit the same boxes.

## Install

Put `zzz-plus-fonts.pk3` (published with the development builds) in the game's `main/` folder.
Remove it to go back to the original fonts.

| Font | Used for | Replacement |
|---|---|---|
| `facfont-20` | HUD, menus | DejaVu Sans Condensed Bold, grey with a dark outline like the original |
| `verdana-12`, `verdana-14` | console, menus, chat | DejaVu Sans |
| `courier-16/18/20` | menus | DejaVu Sans Mono Bold |
| `handle-*`, `marlett` | ornamental titles, symbols | not replaced |

Without the pk3, accented letters are drawn as their base letter (á as a).

## Building it yourself

```
python3 -m pip install pillow
python3 misc/fonts/make_fonts.py --out zzz-plus-fonts.pk3
python3 misc/fonts/make_fonts.py --out zzz-plus-fonts.pk3 --font verdana-14=/path/to/font.ttf
```

`--font` builds only the fonts you list, from any TrueType file you may use (a font's name ends with
its height). `--game-dir <GAME_DIR>` measures the widths of your copy's original fonts;
`--scale` sets the texture resolution (2 by default; higher values need more memory and look
grainier at small sizes, as the game draws fonts without mipmaps).

The DejaVu fonts are under the Bitstream Vera license (DejaVu changes in the public domain); the pk3
carries the notice in `fonts/plus-fonts-license.txt`.
