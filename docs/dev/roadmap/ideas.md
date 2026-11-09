# Feature ideas

Backlog of features beyond upstream, from design discussions. Not started unless marked. Move an
idea to an ADR when it is decided, and to `docs/ai/SYSTEM_SUMMARY.md` when it lands.

## Competition mode (zPAM-like)

Match control for leagues and clan wars, like the maintainer's zPAM Plus for CoD2.

- **Constraint:** MOHAA game modes are C++ (`code/fgame/dm_manager.cpp`), not scripts as in CoD2,
  so a pure script mod cannot control the match flow.
- **Script-only today:** league configs (cvars, `.cfg`), chat commands via `registercmd` and the
  `player_textMessage` event (`docs/markdown/04-coding/02-scripting/01-script-events.md`).
- **Needs C++:** ready-up blocking the start plus countdown, real multiplayer pause / time-outs
  (pause exists only with a single local client), halftime side swap, overtime, enforcing client
  cvars (rate, fov), streamer/spectator view (`cgame`).
- **Shape:** a generic match state layer in `fgame` (warmup, ready-up, live, paused, halftime,
  overtime) exposed as script commands/events; league rules as a script `.pk3` on top.
- **MVP:** ready-up + countdown + halftime swap for TDM and round-based; test on the dedicated
  server with bots.
- **First step:** survey existing MOHAA competition mods/rules to reuse.

## libmoh (extended script functions)

The role libcod plays for CoD2, without injection: the source is available, so new functions are
script events added to `fgame`, portable to every platform.

- **Already present:** file I/O (`fopen`, `fread`, `fwrite`, `fclose`), `md5string`,
  `getcvar`/`setcvar`, `stuffsrv`, `conprintf`, `getdate`/`gettime`, `traced`, `typeof`, `isarray`,
  `registerev`/`unregisterev`, `registercmd`.
- **Missing:** HTTP/web APIs (upstream #671 asks for NightFall's `create_api_request`,
  `register_api_route`, `unregister_api_route`; keep those names for mod compatibility), JSON,
  SQLite, bot control (upstream #684: skills and weapons), player info (ping, IP, GUID, rate), the
  competition-mode primitives above.
- **Shape:** commands grouped in their own files under `code/fgame`; heavy services (HTTP via the
  engine's curl, database) live in the engine and reach the game through new `gi` entries (bump
  `GAME_API_VERSION`, see [boundaries.md](../architecture/boundaries.md)); no external plugin
  `.so` system at first (ABI/versioning cost).
- **Rule:** HTTP must be asynchronous; a script waits for the response like any other event.
- **MVP:** NightFall-compatible `create_api_request` + JSON parsing, tested headless against a
  local HTTP server.
- **First step:** compare NightFall's function spec and libcod-plus's functions one by one.

## Vulkan renderer

- **Today:** `renderer_opengl1` (default, original look) and `renderer_opengl2` (ioquake3's GLSL
  renderer, `BUILD_RENDERER_GL2=OFF`), selected by `cl_renderer`; no Vulkan or Direct3D.
- **Shape:** a new dynamically loaded module `renderer_vulkan` behind `refimport_t`/`refexport_t`
  (`code/renderercommon/tr_public.h`); the rest of the engine stays unchanged.
- **Cost:** the largest idea here; MOHAA-specific rendering (TIKI/skeletal models, terrain, sky
  portals, effects) must be ported. Prior art for Quake III: Quake3e, vkQuake3.
- **Cheap alternatives first:** Mesa's Zink already runs the GL renderer on Vulkan drivers (Linux);
  enabling and testing `opengl2` in builds.
- **Not planned:** Direct3D 12 (the original game was OpenGL; Vulkan covers Windows, Linux and
  macOS via MoltenVK).

## Also discussed

- **Faster server tick:** `sv_fps` above 20 (40 Hz and up) without breaking original clients, as
  OpenCoD2-Plus does.
- **Single binary:** `openmohaa +set dedicated 1` already works as a dedicated server; dropping
  `omohaaded` would need SDL loaded with `dlopen` so headless hosts do not require graphics/audio
  libraries. Recommendation so far: keep both, document `+set dedicated 1`.
- **Saves from the original game** (upstream #601): not listed in Load Game; check whether they can
  be listed and loaded.
