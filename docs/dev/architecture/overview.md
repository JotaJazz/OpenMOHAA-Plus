# Architecture overview

OpenMOHAA Plus is a fork of OpenMoHAA: an ioquake3-based engine plus a reimplementation of the
Medal of Honor: Allied Assault game logic (AA, Spearhead, Breakthrough). C for the engine, C++ for
game logic, UI and the script VM.

## Build outputs

Defined in `cmake/` and the root `CMakeLists.txt`.

| Output | Kind | Built from | CMake file |
|---|---|---|---|
| `openmohaa` | client executable | shared engine + `client`, `sdl`, `uilib`, `gamespy` | `cmake/client.cmake` |
| `omohaaded` | dedicated server executable | shared engine + `null` stubs | `cmake/server.cmake` |
| `game.so` / `game.dll` | server game module (dlopen) | `fgame` (incl. `bg_*`), `script`, `parser`, script system, shared `qcommon` parts | `cmake/basegame.cmake` |
| `cgame.so` / `cgame.dll` | client game module (dlopen) | `cgame`, `bg_*` from `fgame`, script system, shared `qcommon` parts | `cmake/basegame.cmake` |
| `renderer_opengl1.so` | renderer (dlopen when `USE_RENDERER_DLOPEN`) | `renderergl1`, `renderercommon` | `cmake/renderer_gl1.cmake` |
| `renderer_opengl2.so` | renderer, off by default (`BUILD_RENDERER_GL2`) | `renderergl2`, `renderercommon` | `cmake/renderer_gl2.cmake` |
| `launch_openmohaa_{base,spearhead,breakthrough}` | launchers that start `openmohaa` with the right game | `Launcher` | `cmake/launcher.cmake` |

"Shared engine" is `cmake/shared_sources.cmake` (`qcommon`, `server`, `sys`, `tiki`, `skeletor`,
`gamespy`, ...). "Script system" is `cmake/shared_script.cmake`: most of `corepp` plus
`script/scriptvariable.cpp` and `script/scriptexception.cpp`, linked into both executables and
both game modules. The server code is linked into the client too (local games run the server
in-process). Unit tests: `cmake/tests/` (`test_lz77`, built when `BUILD_TESTING` is on); game
regression tests: `tests/`.

## Source tree (`code/`)

| Directory | Role |
|---|---|
| `code/qcommon` | Engine core: cvars, commands, filesystem (pk3), memory zones, network, frame loop (`common.c`) |
| `code/server` | Server: clients, snapshots, map load (`sv_init.c`), game module host (`sv_game.c`), console commands (`sv_ccmds.c`) |
| `code/client` | Client: connection, cgame host (`cl_cgame.cpp`), UI host (`cl_ui.cpp`), sound (`snd_*_new.cpp`, OpenAL) |
| `code/sys` | OS layer: entry point, tty console, dynamic library loading (`code/sys/new/sys_main_new.c`) |
| `code/sdl` | SDL2 window, input, GL context |
| `code/null` | Stubs that replace client/sound/input in the dedicated server |
| `code/fgame` | Server game logic: entities, player, AI, bots, items, script commands, savegames |
| `code/cgame` | Client game logic: HUD, effects, prediction |
| `code/script` | Script VM (`scriptvm.cpp`), compiler, variables (MOHAA `.scr` language) |
| `code/parser` | Flex/Bison grammar for `.scr` (`bison_source.txt`, `lex_source.txt`); the build generates the parser into the gitignored code/parser/generated; `lex.yy.cpp` / `y.tab.cpp` are stale committed copies, not built |
| `code/corepp` | Shared C++ base: class/event/listener system (`listener.cpp`, `class.cpp`), containers (`con_set`, `con_arrayset`); compiled per target and includes the host module's headers |
| `code/tiki`, `code/skeletor` | TIKI model definitions and skeletal animation |
| `code/uilib` | MOHAA UI library (Ubertools `.urc` menus), linked into the client |
| `code/renderercommon`, `code/renderergl1`, `code/renderergl2` | Renderers |
| `code/gamespy` | GameSpy SDK (master server, CD key, NAT negotiation) |
| `code/Launcher` | Launcher executables |
| `code/tools` | Tools: q3asm and lcc (QVM build), stringify, `ommap`, `md5_2_skX` |
| `code/asm` | Float/snap helpers (`cmake/compilers/gnu.cmake`, `cmake/compilers/msvc.cmake`) |
| `code/thirdparty` | Vendored libraries (curl, openal-soft, SDL2, zlib, libmad, ogg/vorbis/opus, jpeg, recastnavigation) |
| `code/curl` | curl declarations for HTTP support (included by `code/sys/sys_curl.h`) |
| `code/web` | Web client page and config |
| `code/botlib`, `code/q3_ui` | ioquake3 leftovers, not part of the build |

Other top-level directories: `cmake/` (build scripts), `docs/` (user docs published with Doxygen
from `docs/markdown`; technical docs in `docs/dev` and `docs/ai`), `container/` (Docker images),
`misc/` (packaging, icons, `misc/docs-check`), `.github/workflows/` (CI).

## Runtime structure

```
openmohaa (client process)                omohaaded (dedicated server)
├── qcommon frame loop  ── Com_Frame      ├── qcommon frame loop
├── server (local games) ─ game.so        └── server ─ game.so
│                           (gi / ge)                  (gi / ge)
├── client ─ cgame.so (cgi / cge)
│         ├─ uilib (in-process)
│         └─ sound: OpenAL Soft
└── renderer_opengl1.so (ri / re)
```

Each module talks to the engine only through a function table: `game_import_t`/`game_export_t`
(`code/fgame/g_public.h`, `GAME_API_VERSION`), `clientGameImport_t`/`clientGameExport_t`
(`code/cgame/cg_public.h`), `refimport_t`/`refexport_t` (`code/renderercommon/tr_public.h`).
Rules: [boundaries.md](boundaries.md). Subsystem notes: [game-and-script.md](game-and-script.md),
[client-audio.md](client-audio.md).
