# OpenMOHAA Plus

![Status: in development](https://img.shields.io/badge/status-in%20development-orange)
![License: GPL-2.0](https://img.shields.io/badge/license-GPL--2.0-blue)
![Platforms: Linux | Windows | macOS](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![MOHAA | Spearhead | Breakthrough](https://img.shields.io/badge/MOHAA-AA%20%7C%20Spearhead%20%7C%20Breakthrough-6b7a3a)
[![Fork of openmoh/openmohaa](https://img.shields.io/badge/fork%20of-openmoh%2Fopenmohaa-555)](https://github.com/openmoh/openmohaa)
[![Builds](https://github.com/JotaJazz/OpenMOHAA-Plus/actions/workflows/branches-build.yml/badge.svg?branch=main)](https://github.com/JotaJazz/OpenMOHAA-Plus/actions/workflows/branches-build.yml)
[![Last commit](https://img.shields.io/github/last-commit/JotaJazz/OpenMOHAA-Plus/main)](https://github.com/JotaJazz/OpenMOHAA-Plus/commits/main)

[Español](README.es.md) · **English**

**Medal of Honor: Allied Assault is still alive.** OpenMOHAA Plus is a fork of
[OpenMoHAA](https://github.com/openmoh/openmohaa), the open-source reimplementation of Medal of
Honor: Allied Assault, Spearhead and Breakthrough: the client, the dedicated server and the game
logic, for Windows, Linux and macOS.

It exists for one reason: **to move forward with AI coding agents**, which the original project
does not accept. Here every change is made with an assistant, reproduced on the real game before
and after, and kept in this fork. On top of OpenMoHAA's work we fix reported bugs and add new
features, the way the sibling project [OpenCoD2-Plus](https://github.com/JotaJazz/OpenCoD2-Plus)
is doing for Call of Duty 2 with its 40 Hz server.

<table>
  <tr>
    <td><img src="docs/assets/images/v0.60.0-x86_64/mohdm1_1.png" alt="Southern France (mohdm1)"></td>
    <td><img src="docs/assets/images/v0.60.0-x86_64/training_1.png" alt="Single-player training"></td>
  </tr>
  <tr>
    <td><img src="docs/assets/images/v0.60.0-x86_64/flughafen_1.png" alt="Flughafen"></td>
    <td><img src="docs/assets/images/v0.60.0-x86_64/mohdm2_1.png" alt="Freeze-Tag with bots"></td>
  </tr>
</table>
<p align="center"><sub>Screenshots from OpenMoHAA; the engine and game code are the same.</sub></p>

## What works today

- **Everything OpenMoHAA does:** the whole single-player campaign of the three games, all official
  multiplayer modes (Free-For-All, Team Deathmatch, Round-based, Objective, Tug-of-War,
  Liberation), mods such as Freeze-Tag, built-in bots, compatibility with the original assets,
  scripts and servers ([differences from the original](docs/markdown/01-intro/04-differences.md)).
- **Fixes for bugs reported upstream**, each one reproduced before and after on the real game:
  - `s_dialogscale` above 1 now raises dialog volume (#885)
  - `com_maxfps` caps at the requested rate: 144 gives 144 fps, not 166 (#530)
  - the HUD stopwatch survives loading a save (#916)
  - map scripts no longer burst all their waits after a same-map restart, which dropped every
    player on single-map servers (#287)
  - no crash on map change when the game module stays loaded, as on Alpine/musl (#918)
  - `sv_pure 1` no longer rejects clients that read an extra pk3 in the menu (#659)
  - `rehashbans` no longer keeps removed bans (#926)
  - script fixes: `registercmd` commands run (#672), item `amount`/`dmamount` are readable (#920),
    `waitForState` waits (#702)
  - sounds whose alias omits `.wav` load (#589), a cursor double free, and executable release
    archives on Linux and macOS (#923)
- **New in Plus:**
  - 40 Hz servers (`sv_fps 40`) and `sv_perf` frame timings
  - script events to change damage and refuse team changes, round events
    ([docs](docs/markdown/04-coding/02-scripting/03-plus-events.md)), HTTP and JSON functions
    compatible with NightFall ([docs](docs/markdown/04-coding/02-scripting/04-plus-libmoh.md)),
    server scripts without overwriting game files (`sv_scripts`), and a
    [stats mod](misc/mods/plus-stats/README.md) built on them
  - [in-game admin accounts](docs/markdown/03-configuration/04-plus-admin.md) with roles and
    per-command permissions, no RCON password needed (`ad_login`)
  - [HUD scale and crosshair size](docs/markdown/03-configuration/05-plus-hud.md) (`ui_hudscale`, `cg_crosshair_size`)
  - automatic mod downloads over HTTP (`sv_dlURL`), the reason shown when a server drops you,
    HRTF audio for headphones (`s_hrtf 1`)
  - a script checker and a VS Code extension ([misc/scr-tools](misc/scr-tools),
    [misc/vscode-mohaa](misc/vscode-mohaa)), freeze and crash reports, a headless test suite

  Numbers are [upstream issues](https://github.com/openmoh/openmohaa/issues). The full list, with
  the risks each one brings, is in [docs/ai/SYSTEM_SUMMARY.md](docs/ai/SYSTEM_SUMMARY.md) and
  [docs/dev/roadmap/debt.md](docs/dev/roadmap/debt.md).

## What comes next

New features beyond the original game, in the spirit of OpenCoD2-Plus. Planned, not done yet:

- **Competition mode** (zPAM-like) on more gametype events: team scores, halftime side swaps.
- **Better bots** and lag compensation.
- **More upstream bugs**, picked by what can be reproduced and verified.
- **Same fix for the client module on musl** that the server got in #918.

> [!NOTE]
> **Still in development.** Expect bugs and missing pieces. Behaviour changes against the original
> are listed in [docs/dev/roadmap/debt.md](docs/dev/roadmap/debt.md).

> [!IMPORTANT]
> **No game content is included: bring your own data.** This repository is source code only. To
> play you need the files of a copy of Medal of Honor: Allied Assault (and optionally Spearhead and
> Breakthrough) that **you legally own**.

## How to play

Development builds are published automatically on the
[`dev` pre-release](https://github.com/JotaJazz/OpenMOHAA-Plus/releases/tag/dev). Otherwise build
it (see [Building](#building)). Installation works as in OpenMoHAA:

- [Installing](docs/markdown/01-intro/01-installation.md), [running the game and expansions](docs/markdown/02-running/01-running.md), [FAQ](docs/markdown/02-running/03-faq.md)
- [Setting up a server](docs/markdown/02-running/02-running-server.md), [bots](docs/markdown/03-configuration/03-configuration-bots.md), [configuration](docs/markdown/03-configuration/01-configuration.md)

From a build, point it at your game folder, the one that contains `main/` (not needed when the
build sits in that folder, for a GOG install on Windows, or for the original installer's
`EA GAMES\MOHAA` folder, also inside the default Wine prefix):

```sh
./openmohaa +set fs_basepath /path/to/MOHAA
./omohaaded +set fs_basepath /path/to/MOHAA +map dm/mohdm1     # dedicated server
```

## For contributors

Playing and reporting bugs, testing on other systems, or code: every kind of help counts. Do not
send this fork's changes to the original repository: its rules forbid AI-generated contributions.
The technical documentation, in English, starts at [docs/dev/README.md](docs/dev/README.md);
[AGENTS.md](AGENTS.md) is the entry point for AI coding agents.

## Building

CMake and Ninja. Dependencies for every platform:
[docs/markdown/04-coding/01-compiling.md](docs/markdown/04-coding/01-compiling.md). On Linux:

```sh
cmake -S . -B .cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=$HOME/omohaa
cmake --build .cmake
cmake --install .cmake        # -> $HOME/omohaa/lib/openmohaa: openmohaa, omohaaded, game.so, cgame.so...
```

More in [docs/dev/development/compiling.md](docs/dev/development/compiling.md).

## Testing

```sh
python3 -I -m unittest discover -s misc/docs-check     # docs checker tests
python3 -I misc/docs-check/check_docs.py               # docs checker
OMOH_GAME_DIR=/path/to/MOHAA python3 -I -m unittest discover -s tests   # game tests, headless
```

Bugs are verified on the real binaries without opening windows or playing sound: offscreen
client, null audio, scripted console input, bots, screenshots and audio rendered to a file. The
recipes are in [docs/dev/development/testing.md](docs/dev/development/testing.md).

## Repository layout

The source tree, the build outputs and how the engine, the game modules and the renderer fit
together: [docs/dev/architecture/overview.md](docs/dev/architecture/overview.md).

## Transparency

- **Who maintains this:** I'm JotaJazz. I like old games, and I want to keep playing them with my
  friends and keep them alive for the community, with open projects. This one sits next to
  [OpenCoD2-Plus](https://github.com/JotaJazz/OpenCoD2-Plus).
- **How the work is done:** I work with [Claude Code](https://claude.com/claude-code), Anthropic's
  coding assistant. The changes in this fork, including this README, are written with its help.
  Commits made this way carry a `Co-Authored-By: Claude` line, so they can be found with `git log`.
- **Relationship with the original:** the engine and game reimplementation are the work of the
  OpenMoHAA contributors. This fork follows their `main` and merges it in; it never sends its own
  changes back, because OpenMoHAA does not accept AI-generated contributions
  ([ADR 0001](docs/dev/architecture/adr/0001-fork-keeps-ai-assisted-work.md)). Please report bugs
  of this fork here, not to OpenMoHAA.
- **What it is not:** it is not an official project and is not affiliated with Electronic Arts,
  2015 Inc. or the OpenMoHAA team. It contains no game files.

## Notice

OpenMOHAA Plus is an independent project. It is not affiliated with, authorized by, sponsored by,
or endorsed by Electronic Arts Inc. or any of its affiliates. "Medal of Honor" is a trademark of
Electronic Arts Inc., used here only to identify the game the code is compatible with.

This project does not provide or help obtain copyrighted game data. The screenshots show content
of Medal of Honor: Allied Assault, property of Electronic Arts Inc., for identification only.

## License

GPL-2.0, as OpenMoHAA ([COPYING.txt](COPYING.txt)). Third-party libraries keep their own licenses:
[Flex](https://github.com/westes/flex), [Bison](https://savannah.gnu.org/projects/bison/),
[SDL](https://www.libsdl.org/), [OpenAL Soft](https://openal-soft.org/),
[libmad](https://www.underbit.com/products/mad/), [cURL](https://curl.se/),
[libogg and libvorbis](https://xiph.org/vorbis/), [Opus](https://opus-codec.org/),
[Recast Navigation](https://github.com/recastnavigation/recastnavigation), zlib and libjpeg
(sources in `code/thirdparty`).

## Credits

- [OpenMoHAA](https://github.com/openmoh/openmohaa) and its contributors, the reimplementation this fork is built on
- [ioquake3](https://github.com/ioquake/ioq3) and id Software, the engine
- Ritual Entertainment's [F.A.K.K. 2 SDK](https://code.idtech.space/ritual/fakk2-sdk)
- 2015 Inc. and Electronic Arts, Medal of Honor: Allied Assault

## Resources

[MOH-DB](https://www.moh-db.com/) · [333networks](https://333networks.com/) ·
[ModDB](https://www.moddb.com/games/medal-of-honor-allied-assault) ·
[GameBanana](https://gamebanana.com/games/720) ·
[OpenMoHAA documentation](https://openmoh.github.io/openmohaa) ·
[OpenMoHAA Discord](https://discord.gg/NYtH58R) (upstream community)
