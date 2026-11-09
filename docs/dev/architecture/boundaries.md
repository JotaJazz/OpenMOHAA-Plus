# Architectural boundaries

Normative. "Must" rules apply to new code; existing violations are listed as accepted exceptions.
Derived from `#include "../<dir>/..."` usage and the CMake targets in [overview.md](overview.md).

## Layers

| Layer | Directories | Ships in |
|---|---|---|
| Engine | `qcommon`, `server`, `client`, `sys`, `sdl`, `null`, `uilib` | executables |
| Shared C++ base | `corepp`, `tiki`, `skeletor` | executables and modules (compiled per target) |
| Game modules | `fgame` (+ `script`, `parser`), `cgame` | `game.so`, `cgame.so` |
| Renderers | `renderercommon`, `renderergl1`, `renderergl2` | `renderer_opengl*.so` |
| Vendored | `thirdparty`, `gamespy` | linked in |

## Allowed dependencies

`Y` = may include headers of; `api` = only the public interface header(s) listed below.

| from \ to | qcommon | engine (server/client/sys) | corepp/tiki | fgame | cgame | renderer |
|---|---|---|---|---|---|---|
| qcommon | Y | Y (legacy) | Y | - | - | - |
| server | Y | Y | Y | api | - | - |
| client | Y | Y | Y | api | api | api |
| fgame / script | Y (shared parts) | - | Y | Y | - | - |
| cgame | Y (shared parts) | - | Y | api (`bg_*`) | Y | api (`tr_types.h`) |
| renderer | Y | - | Y | - | - | Y |
| corepp / tiki | Y | per target | Y | per target (`GAME_DLL`) | per target (`CGAME_DLL`) | - |

Public interface headers:
- Game: `code/fgame/g_public.h`, `code/fgame/bg_public.h` (shared player movement and constants).
- Client game: `code/cgame/cg_public.h`, `code/cgame/tr_types.h`.
- Renderer: `code/renderercommon/tr_public.h`.
- UI: `code/uilib/ui_public.h`.

## Rules

1. A module (`fgame`, `cgame`, renderer) must reach the engine only through its import table
   (`gi`, `cgi`, `ri`). It must not include `code/server`, `code/client` or `code/sys` headers.
2. The engine must reach a module only through its export table (`ge`, `cge`, `re`).
3. Changing a field order or signature in an import/export table is an ABI change: bump the
   version constant (`GAME_API_VERSION`, `CGAME_IMPORT_API_VERSION`, `REF_API_VERSION`) in the
   same commit.
4. Memory allocated by a module (`TAG_GAME`, `TAG_CGAME`) is owned by that module's lifetime.
   The engine may only bulk-free it after the module is really unloaded (see ADR
   [0004](adr/0004-module-may-stay-resident.md)).
5. Savegame layout (`Archiver` in `code/fgame/archive.cpp`, `SV_ArchiveLevel`) is a persistent
   format: changes must keep old saves loadable or be gated by a version check.
6. Original-game behaviour is the default; deviations are marked in code with the markers defined
   in `CONTRIBUTING.md` (`Fixed in OPM`, `Added in OPM`, `Changed in OPM`, `Removed in OPM`) and a
   reason (ADR [0002](adr/0002-original-behaviour-by-default.md)).
7. Vendored code in `code/thirdparty` and `code/gamespy` is not edited in place except for
   build fixes; update through `misc/update-libs.sh` where applicable.

## Accepted exceptions (existing coupling)

| Exception | Where | Why it stays |
|---|---|---|
| `script` includes `fgame` headers (`scriptthread.h`, `scriptmaster.h`, `level.h`, ...) | `code/script` | The VM proper is built only into `game.so`; only `scriptvariable.cpp` / `scriptexception.cpp` are shared (`cmake/shared_script.cmake`) |
| `corepp` includes the host module's headers (`../fgame/g_local.h`, `../cgame/...`, `../client/client.h`) | `code/corepp` | Compiled once per target; selected by `GAME_DLL` / `CGAME_DLL` ifdefs |
| `qcommon` includes `client`/`server` headers | `code/qcommon` | ioquake3 heritage (frame loop drives both) |
| `cgame` includes `../client/keycodes.h` | `code/cgame/cg_ui.cpp` | Key constants only |
| `uilib` includes `client` headers | `code/uilib` | UI library runs inside the client, not as a module |
| `tiki` includes `client`/`server` headers | `code/tiki` | Shared by engine and modules; compiled per target |
| `CGAME_HOOK` include of a removed directory | `code/fgame/glb_local.h`, `code/fgame/worldspawn.h` | Dead code behind an undefined macro |
| Engine frees `TAG_CGAME` unconditionally after unloading cgame | `CL_ShutdownCGame` in `code/client/cl_cgame.cpp` | Violates rule 4 on musl; open in [debt.md](../roadmap/debt.md) |

## Forbidden

- New includes from a module into engine-private headers.
- Calling engine functions from a module through any path other than its import table.
- Freeing module-owned memory from the engine while the module may still be resident.
- Editing parser output by hand: the build generates it into code/parser/generated (gitignored)
  from `code/parser/bison_source.txt` / `code/parser/lex_source.txt` (`flex_target` /
  `bison_target` in `cmake/basegame.cmake`). `code/parser/lex.yy.cpp` and `code/parser/y.tab.cpp`
  are stale committed copies that are not built.
