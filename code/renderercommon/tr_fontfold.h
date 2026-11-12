/*
===========================================================================
Copyright (C) 2026 the OpenMOHAA Plus contributors

This file is part of OpenMOHAA Plus source code.

OpenMOHAA Plus source code is free software: you can redistribute it
and/or modify it under the terms of the GNU Affero General Public License
as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

OpenMOHAA Plus source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with OpenMOHAA Plus source code.  If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/

// tr_fontfold.h: Added in OpenMOHAA Plus
//  The game's fonts only hold printable ASCII. A Latin-1 / Windows-1252 character without a glyph
//  is drawn with the glyph of its base letter (a for á, n for ñ, ! for ¡) instead of '?', so
//  translations stay readable with the original fonts; a font that has the glyph keeps it.

#pragma once

static inline void R_FoldMissingGlyphs(int indirection[256])
{
    // 0xA0-0xFF
    static const char latin1[] = " !cLoY|S\"ca\"--r-"
                                 "o+23'uP.,1o\"423?"
                                 "AAAAAAACEEEEIIII"
                                 "DNOOOOOxOUUUUYTs"
                                 "aaaaaaaceeeeiiii"
                                 "dnooooo/ouuuuyty";
    // Windows-1252 punctuation in 0x80-0x9F
    static const struct {
        unsigned char code;
        char          base;
    } cp1252[] = {
        {0x85, '.'}, {0x91, '\''}, {0x92, '\''}, {0x93, '"'}, {0x94, '"'}, {0x96, '-'}, {0x97, '-'}
    };
    int i;

    for (i = 0xA0; i <= 0xFF; i++) {
        if (indirection[i] == -1) {
            indirection[i] = indirection[(unsigned char)latin1[i - 0xA0]];
        }
    }
    for (i = 0; i < (int)(sizeof(cp1252) / sizeof(cp1252[0])); i++) {
        if (indirection[cp1252[i].code] == -1) {
            indirection[cp1252[i].code] = indirection[(unsigned char)cp1252[i].base];
        }
    }
}
