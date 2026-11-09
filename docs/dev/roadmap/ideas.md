# Feature ideas

Backlog of features beyond upstream, from design discussions. Not started unless marked. Move an
idea to an ADR when it is decided, and to `docs/ai/SYSTEM_SUMMARY.md` when it lands.

## Automated test suite

No project test runs in CI today (`test_lz77` exists but is not wired in; see
[debt.md](debt.md)). Every fix so far was verified with hand-written headless commands
([testing.md](../development/testing.md)).

- **Shape:** turn those recipes into reusable Python scripts (stdlib), like OpenCoD2-Plus's:
  server smoke with bots and map rotation, save/load with screenshot check, `registercmd`, bans
  (`rehashbans`), `sv_pure`, audio rendered through the OpenAL wave backend, client `vid_restart`.
- **Data:** tests needing game data take a `<GAME_DIR>` argument and run locally; CI runs what
  needs no data (unit tests, docs checker) and, if feasible, tests on generated minimal assets.
- **Also:** wire `test_lz77` into CTest.
- **Why first:** every later fix gets regression protection and an agent can verify its own work.

## Robustness: fuzzing and sanitizers

Two real memory bugs were found by chance (`TAG_GAME` freed under a resident module, cursor double
free).

- **Shape:** AddressSanitizer/UBSan builds plus fuzzing of inputs that come from outside: network
  packets, the `.scr` compiler (`code/parser`, `code/script`), pk3/zip, `.tik`, `.urc`,
  `serverbans.dat`, configstrings.
- **Why:** these bugs crash public servers and are found almost automatically.

## Freeze watchdog and crash reports

A hung or crashed server leaves little to go on (the #287 backtrace had no symbols).

- **Shape:** a watchdog that reports frames stuck longer than a threshold (like OpenCoD2-Plus's
  `com_freezeWatch`) and a readable backtrace written to a file on fatal signals, with build
  version and map.
- **Why:** helps server admins report bugs and us reproduce them.

## Widescreen and HUD scaling

Popular upstream requests: #583 (widescreen), #585 (HUD scale cvar), #911 (crosshair scale
independent of texture size).

- **Shape:** client-side (`cgame`, `uilib`, renderer 2D); keep original layout as default.
- **Test:** headless screenshots at several resolutions and aspect ratios.

## Server administration

Upstream #905 asks for an ACL-based admin system with named permissions.

- **Shape:** accounts and permissions, per-command authorization for rcon and chat commands,
  player info (IP, GUID, ping), better bans and logs. Shares primitives with libmoh and the
  competition mode.

## Gametype scripting API (CoD2-style callbacks)

The MOHAA community works mostly in scripts. CoD2 lets scripts own the whole gametype through
engine callbacks; OpenMoHAA scripts can only observe: `player_damaged` and `player_killed` fire
after the fact, and only `player_textMessage` can block something (its return value). No new
language is needed: the existing `.scr` VM is enough; what is missing is the engine handing
decisions to scripts.

- **Level 1, deciding events (low-medium):** events that fire before the action and accept a
  return value, like `player_textMessage`: `player_damaged` returns the final damage (0 = none),
  team-change and spawn-point events that can refuse or choose; new `round_start`, `round_end`,
  `match_start`. One small C++ change per event, each testable headless with bots.
- **Level 2, control primitives (medium):** script commands to end a round and declare a winner,
  set player/team scores, move a player to a team, force a respawn, pause. With levels 1 and 2 a
  script can drive most of a match while the C++ modes stay the default.
- **Level 3, fully scripted gametype (high):** a mode where `code/fgame/dm_manager.cpp` delegates
  everything to a script; FFA and TDM rewritten in script as reference. Risks: client scoreboard and
  HUD expect specific server data, and diverging from upstream `dm_manager`.
- **Recommendation:** levels 1 and 2 first; they are the base for the competition mode and libmoh.
- **First step:** map how coupled `dm_manager.cpp` is (round flow, scoring, spawning) and list the
  decision points to expose.

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
  overtime) exposed as script commands/events; league rules as a script `.pk3` on top. Builds on the
  gametype scripting API (levels 1-2) above.
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

## Campaign co-op

Play the single-player missions together. Interest exists (the HaZardModding Coop mod, upstream
#932).

- **Constraint:** SP level scripts assume a single `$player`; loading an SP map in a multiplayer
  gametype floods the log with script errors (see upstream #422 in [debt.md](debt.md)).
- **Shape:** engine support for several players in SP levels (spawning, `$player` resolution,
  checkpoints, save/load policy) plus script adaptations; study the existing coop mod first.
- **Cost:** large, high community impact.

## Competitive netcode: lag compensation and hit validation

- **Shape:** server-side lag compensation when resolving shots (like Quake III "unlagged"), with
  validation against impossible hits; pairs with the faster server tick and the competition mode.
- **First step:** measure current hit registration with bots and simulated latency
  (`cl_packetdelay`, `sv_packetdelay` exist).

## Match demos

- **Shape:** server-side recording of matches for clan wars review, streaming and cheat reports.
- **First step:** check what OpenMoHAA supports today for demo recording and playback (the stock
  game folder has a `main/demos` directory).

## Automatic mod download over HTTP

Joining a modded server usually means finding its pk3 files by hand.

- **Shape:** the client downloads the pk3 files a server requires from an HTTP URL, as
  OpenCoD2-Plus does with `sv_wwwBaseURL`; the engine already has curl (`USE_HTTP`) and
  `cl_allowDownload`.
- **First step:** verify the current download flow end to end with a local HTTP server; the server
  must only offer paks it really has.

## Game install detection

- **Shape:** use the existing store paths (`fs_steampath`, `fs_gogpath`, `fs_microsoftstorepath`)
  and a first-run prompt for the game folder, so players do not need `+set fs_basepath`.
- **Why:** lowers the barrier for new players.

## Performance profiling

- **Shape:** integrate a profiler (upstream PR #865 proposes Tracy) to measure server frame cost
  before raising the tick rate or adding a renderer.

## Also discussed

- **Faster server tick:** `sv_fps` above 20 (40 Hz and up) without breaking original clients, as
  OpenCoD2-Plus does.
- **Single binary:** `openmohaa +set dedicated 1` already works as a dedicated server; dropping
  `omohaaded` would need SDL loaded with `dlopen` so headless hosts do not require graphics/audio
  libraries. Recommendation so far: keep both, document `+set dedicated 1`.
- **Saves from the original game** (upstream #601): not listed in Load Game; check whether they can
  be listed and loaded.
