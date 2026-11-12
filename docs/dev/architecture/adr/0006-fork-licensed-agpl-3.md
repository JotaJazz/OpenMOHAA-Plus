# 0006. The fork is distributed under AGPL-3.0

- Status: Accepted
- Date: 2026-11-11

## Context
OpenMoHAA is GPL-2.0. Every GPL header in `code/` (OpenMoHAA, ioquake3, F.A.K.K. 2 SDK parts) says
"version 2 of the License, or (at your option) any later version"; none is GPL-2.0-only. The
bundled libraries are permissive (SDL, zlib, Recast, curl, Opus, Ogg/Vorbis, libjpeg), LGPL
(OpenAL Soft) or GPL-2.0-or-later (libmad). The fork's changes matter most on servers (admin,
antilag, tick rate, server scripts), where GPL asks nothing of whoever runs a modified binary
for other players. The sibling OpenBF1942-Plus uses AGPL-3.0.

## Decision
Files written only by OpenMOHAA Plus are AGPL-3.0-or-later (`LICENSE`): C and C++ files carry
the AGPL header block, other sources a copyright line plus
`SPDX-License-Identifier: AGPL-3.0-or-later`. Files without comments (JSON, READMEs) in the
fork-only directories `code/renderervk`, `misc/docs-check`, `misc/fonts`, `misc/i18n`,
`misc/linux`, `misc/mods`, `misc/scr-tools`, `misc/vscode-mohaa` and `tests` are covered by
this ADR. Upstream files keep their GPL-2.0-or-later header and `COPYING.txt`, also where the
fork changed them. The program as a whole is distributed under AGPL-3.0, which GPL-3.0 section
13 allows for a combination of GPL-3.0 and AGPL-3.0 code.

## Consequences
- Whoever runs a modified version as a server for others must offer them its source.
- Upstream changes still merge in (GPL-2.0-or-later code can be used under GPL-3.0); code that
  is GPL-2.0-only can no longer be added.
- New fork files get the AGPL header (`docs/ai/DEVELOPMENT_GUIDE.md`, recipes).
- Not settled by this ADR: the GameSpy SDK (`code/gamespy`) has no license notice, a gap
  inherited from upstream; `code/tools/lcc` has its own licence but is a build tool, not shipped.
- `code/renderervk/vk_core.c` follows the order of Quake3e's device setup (GPL-2.0, no file
  header); 7 of its 686 non-trivial lines match Quake3e's `vk.c`, all Vulkan API boilerplate,
  so no Quake3e code is copied.
