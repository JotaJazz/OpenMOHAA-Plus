# Feature ideas

Backlog of features beyond upstream, from design discussions. Not started unless marked. Move an
idea to an ADR when it is decided, and to `docs/ai/SYSTEM_SUMMARY.md` when it lands.

## Prioritized plan

Ordering rules: foundations that make every later change safer come first; then cheap wins; then
the script platform, because the community works in scripts and several features build on it;
large bets last. Upstream bug fixing continues in parallel through every phase.

### Clusters (overlaps and dependencies)

| Cluster | Ideas (sections below) | Relationship |
|---|---|---|
| Quality loop | Automated test suite, Mod compatibility tests, Robustness, Automated upstream sync | Mod compatibility tests are part of the suite; upstream sync is only safe once the suite exists |
| Observability | Freeze watchdog, Performance profiling | Same goal (see what the server does); profiling must precede the faster tick and any renderer work |
| Security / anti-cheat | Server flood protection, Robustness, Server-side anti-wallhack, Competitive netcode (hit validation) | Fuzzing and flood limits harden inputs; anti-wallhack and hit validation are server-authoritative anti-cheat |
| Script platform | Gametype scripting API, libmoh, Script modding toolkit | The API and libmoh are the engine side; the toolkit documents both (completion and docs generated from the event dump) |
| Competitive | Faster server tick, Competitive netcode, Competition mode, Server-controlled QoL, Map voting, Better bots | Competition mode, QoL and map voting are scripts on the gametype API; netcode pairs with the tick rate; bots give practice and test players |
| Online services | libmoh HTTP/JSON, Match statistics, Server administration, Master server portal | Stats and admin consume libmoh; the portal needs stats; admin shares player-info primitives with competition mode |
| Joining a server | Helpful connection errors, Automatic mod download, Reliable server browser, Game install detection | Mod download removes most "Unpure" drops; errors explain the rest |
| HUD and text | Widescreen and HUD scaling, Sharp text | Same 2D drawing code (cgame, uilib, renderer 2D); do together |
| Distribution | Fork identity, Linux packaging, WebAssembly build, Single binary, Translated README | Packages and stores need the fork's own versions and update checker first |
| Languages | Translations, Translated README | README translations are cheap docs work; in-game translations reuse the localization files |
| Standalone | HRTF spatial audio, Match demos, Campaign co-op, Vulkan renderer, saves from the original game (#601) | Co-op benefits from the gametype API; Vulkan needs profiling |

### Phases

| Phase | Work, in order | Why here |
|---|---|---|
| 0. Foundations and quick wins | Fork identity (update checker, versions, stable releases; also the CI items in [debt.md](debt.md)) · Automated test suite (first scripts + `test_lz77`) · Freeze watchdog and crash reports · HRTF spatial audio · Helpful connection errors | Small or foundational; the update checker currently sends players to upstream; tests protect everything after |
| 1. Quality loop | Robustness (sanitizers, fuzzing) · Server flood protection · Performance profiling · Automated upstream sync · Mod compatibility tests | Needs the test suite; makes public servers safer; profiling is a prerequisite of phase 3 |
| 2. Script platform | Gametype scripting API (levels 1-2) · libmoh MVP (NightFall-compatible HTTP + JSON) · Script modding toolkit | The fork's main differentiator for a script-centric community; base of phases 3 and 4 |
| 3. Competitive and online | Faster server tick · Competition mode · Server-controlled QoL · Map voting · Server administration · Match statistics and integrations · Better bots · Competitive netcode · Server-side anti-wallhack | Built on phases 1-2; ordered from most reused to most costly |
| 4. Player experience and reach | Automatic mod download · Reliable server browser · Game install detection · Widescreen + Sharp text · Linux packaging · Translations + Translated README · Match demos · Saves from the original game (#601) | Broadens the audience once the online core is solid; README translations can be pulled earlier when the README stabilizes |
| 5. Big bets | Campaign co-op · WebAssembly build · Vulkan renderer · Single binary · Master server and community portal · Gametype API level 3 | High cost or dependent on everything above |

Progress per phase is live state: [SYSTEM_SUMMARY.md](../../ai/SYSTEM_SUMMARY.md#open-work). Every branch
named in a "Status" line below is merged into `main`.

## Automated test suite

CI runs only `test_lz77` (CTest, `unit-testing.yml`) and the docs checker; the game tests in
`tests/` need game data and run locally ([testing.md](../development/testing.md)).

- **Shape:** turn those recipes into reusable Python scripts (stdlib), like OpenCoD2-Plus's:
  server smoke with bots and map rotation, save/load with screenshot check, `registercmd`, bans
  (`rehashbans`), `sv_pure`, audio rendered through the OpenAL wave backend, client `vid_restart`.
- **Data:** tests needing game data take a `<GAME_DIR>` argument and run locally; CI runs what
  needs no data (unit tests, docs checker) and, if feasible, tests on generated minimal assets.
- **Why first:** every later fix gets regression protection and an agent can verify its own work.

## Robustness: fuzzing and sanitizers

Two real memory bugs were found by chance (`TAG_GAME` freed under a resident module, cursor double
free).

- **Shape:** AddressSanitizer/UBSan builds plus fuzzing of inputs that come from outside: network
  packets, the `.scr` compiler (`code/parser`, `code/script`), pk3/zip, `.tik`, `.urc`,
  `serverbans.dat`, configstrings.
- **Why:** these bugs crash public servers and are found almost automatically.

## Freeze watchdog and crash reports

Status: implemented on branch `feat/freeze-watchdog` (POSIX backtraces; Windows logs the freeze
without a stack).

A hung or crashed server leaves little to go on (the #287 backtrace had no symbols).

- **Shape:** a watchdog that reports frames stuck longer than a threshold (like OpenCoD2-Plus's
  `com_freezeWatch`) and a readable backtrace written to a file on fatal signals, with build
  version and map.
- **Why:** helps server admins report bugs and us reproduce them.

## Widescreen and HUD scaling

Popular upstream requests: #583 (widescreen), #585 (HUD scale cvar), #911 (crosshair scale
independent of texture size).

Status: `ui_hudscale` (#585) and `cg_crosshair_size`/`cg_crosshair_scale` (#911) on branch
`feat/hud-scale` (`docs/markdown/03-configuration/05-plus-hud.md`). The FOV is already Hor+
(`CG_CalcFov`). Menus: `ui_menuaspect 1` (default, branch `feat/menu-aspect`) uses one scale for
both axes in `UIWidget::SetVirtualScale`; menu containers already center themselves and their
background fill draws the side bars. Cinematics are not covered.

- **Shape:** client-side (`cgame`, `uilib`, renderer 2D); keep original layout as default.
- **Test:** headless screenshots at several resolutions and aspect ratios.

## Server administration

Finding: player information is already readable from scripts (`getip`, `getping`, `getclientnum`,
`netname`, `kills`, `deaths`, `userinfo`); the missing parts are accounts, permissions (upstream
#905) and per-command authorization.

Upstream #905 asks for an ACL-based admin system with named permissions.

- **Shape:** accounts and permissions, per-command authorization for rcon and chat commands,
  player info (IP, GUID, ping), better bans and logs. Shares primitives with libmoh and the
  competition mode.

## Gametype scripting API (CoD2-style callbacks)

Status: level 1 started on branch `feat/gametype-events`: `player_damage` and `player_teamchange`
(deciding), `match_start`, `round_start`, `round_end`
(`docs/markdown/04-coding/02-scripting/03-plus-events.md`). Level 2: `team_swap` (halftime) on branch
`feat/team-swap`, `team_getscore` made readable; the rest of level 2 already existed (`teamwin`,
`team_setscore`, `join_team`, `respawn`, `addkills`); pause is still missing. Still open: spawn point event, "any
veto wins" semantics for several subscribers, `Spectator` path for team changes, level 2.

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

Finding (2026-11-09, from code, `code/fgame/dm_manager.h` and `code/fgame/player.cpp`): the original
game already has a ready-up. With `g_forceready 0` (default 1) a round-based match waits until every
player on both teams has typed `ready` (`notready` undoes it); `DM_Team::IsReady` gates
`WaitingForPlayers`. A competition MVP can therefore be mostly a script mod: `g_forceready 0`, the
round events of the gametype scripting API for halftime and overtime, and `setteam` for side swaps.
Not verified at run time yet (bots never type `ready`).

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

Status: MVP on branch `feat/libmoh-http`: `create_api_request` (get/post), `json_parse`,
`json_stringify`, NightFall-compatible (`docs/markdown/04-coding/02-scripting/04-plus-libmoh.md`);
engine side `code/server/sv_http.cpp` behind new `gi.HTTP_*` entries. Still open:
`register_api_route` (HTTP server), SQLite, bot control, player info.

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

Status: first stage on branch `feat/vulkan-renderer`: `renderer_vulkan`, the GL1 renderer on a Vulkan
emulation of its GL calls ([ADR 0005](../architecture/adr/0005-vulkan-renderer-emulates-gl1.md));
menus and maps match GL1 headless; in a window on NVIDIA (Linux); Linux and Windows release builds
ship it. Played in a window on Windows (NVIDIA). Next: macOS (MoltenVK), then performance (frames in flight,
batching).

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

Status: lag compensation for bullets implemented on branch `feat/lag-compensation` (`g_antilag`);
hit validation not started.

- **Shape:** server-side lag compensation when resolving shots (like Quake III "unlagged"), with
  validation against impossible hits; pairs with the faster server tick and the competition mode.
- **First step:** measure current hit registration with bots and simulated latency
  (`cl_packetdelay`, `sv_packetdelay` exist).

## Match demos

- **Shape:** server-side recording of matches for clan wars review, streaming and cheat reports.
- **First step:** check what OpenMoHAA supports today for demo recording and playback (the stock
  game folder has a `main/demos` directory).

## Automatic mod download over HTTP

Status: UDP downloads already worked; HTTP redirection implemented on branch
`feat/http-downloads` (`sv_allowDownload 1` + `sv_dlURL "http://host/path"` on the server, any web
server serving the server's home folder; `cl_allowDownload 1` on the client). Only paks the server
references are offered (ioquake3 behaviour): client-only mods belong in an `fs_game` folder.

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
- **Status:** branch `feat/install-detection`: the Windows store lookups searched Quake 3 (Steam app
  2200, GOG 1441704920, Microsoft Store "Quake 3"); GOG now uses MOHAA War Chest (1207659126) and
  Steam and the Microsoft Store are skipped (MOHAA is not sold there). `fs_eapath`: the original
  installer folder `Program Files\EA GAMES\MOHAA`, also in the Wine prefix on Linux and macOS, when the
  base path has no data. Without data the error lists the searched folders and explains
  `fs_basepath`. Open: the EA installer's registry key (value name unverified), a first-run folder
  picker.

## Performance profiling

Status: first step on branch `feat/server-perf`: `sv_perf [reset]` reports microsecond timings of
game frames and snapshot sending (avg, max, histogram). Measured on dm/mohdm1 with bots (one
machine, 30 s each): game frame avg 0.19 ms (4 bots) / 0.30 ms (8 bots) at sv_fps 20, 0.12 /
0.24 ms at sv_fps 40, max 3.1 ms, against a 25 ms budget at 40 Hz. CPU is not the limit for a
faster tick; snapshot cost with real clients and gameplay correctness at 40 Hz are still open.

- **Shape:** integrate a profiler (upstream PR #865 proposes Tracy) to measure server frame cost
  before raising the tick rate or adding a renderer.

## Server scripts (sv_scripts)

Status: implemented on branch `feat/server-scripts`. `sv_scripts` lists scripts started on every
level after the map script, so server mods (stats, competition rules, votes) need not override
stock files. Base for script-only mods of the competition, statistics and voting ideas.

## Script modding toolkit

Status: implemented on branch `feat/scr-toolkit`: `misc/scr-tools/scr_check.py` (compile check with
the real compiler, `file:line: error:` output), `misc/scr-tools/gen_editor_data.py` (command list
from the engine's `dumpallclasses`) and the VS Code extension `misc/vscode-mohaa`. Found and fixed on
the way: `compilescript` aborted the server on a parse error (branch `fix/compilescript-crash`).

The community works in `.scr`; today a script error shows only when the map loads.

- **Existing pieces:** the in-game `compilescript` command (`G_CompileScript` in
  `code/fgame/gamecmds.cpp`), `dumpevents` / `dumpallclasses`, the generated class reference
  (`docs/markdown/04-coding/02-scripting/g_allclasses.html`).
- **Shape:** a command-line `.scr` checker that reuses the real parser without starting the game,
  and a VS Code extension (syntax highlighting, completion and hover docs generated from the
  event dump).

## HRTF spatial audio

Status: implemented on branch `feat/hrtf` (`s_hrtf 1`). Correction to the note below: HRTF was
already allowed with `s_speaker_type 1`, but only when OpenAL detects headphones.

- **Fact:** OpenAL Soft supports HRTF, but the engine disables it on purpose
  (`ALC_HRTF_SOFT` in `code/client/snd_openal_new.cpp`).
- **Shape:** an opt-in cvar (e.g. `s_hrtf 1`) for headphone players.
- **Test:** render audio through the OpenAL wave backend and compare left/right channels.

## WebAssembly build

- **Fact:** `code/web` is an ioquake3 leftover (its config lists Quake III files) and CMake has no
  Emscripten support; OpenCoD2-Plus has a web build.
- **Shape:** browser build that loads the player's own pk3 files. Medium-large.

## Match statistics and integrations

Status: reference script mod `misc/mods/plus-stats` on branch `feat/stats-mod` (which merges
`feat/server-scripts`, `feat/gametype-events` and `feat/libmoh-http`): kills per player and round
results POSTed as JSON at every `round_end`. Discord webhooks need a plain-JSON body (the POST body
is NightFall's typed JSON), so they need a small relay or a raw-body option.

- **Shape:** per-match stats as JSON (kills, accuracy, weapons), Discord webhooks (match start/end,
  joins), a web scoreboard. Built on libmoh's HTTP.

## Map voting and dynamic rotation

Finding (from code): player votes exist: `callvote map|nextmap|restart|g_gametype|kick` and `vote`
(`code/fgame/player.cpp`, `code/fgame/clientvote.cpp`), on by default (`g_allowvote 1`), with
the vote menu options read from `callvote.cfg`. Missing: an automatic vote among a few maps at the
end of a match, and rotations by player count.

- **Shape:** vote for the next map at the end of a match; rotations that adapt to the player count.
  Script on top of the gametype scripting API, or C++.

## Helpful connection errors

Status: first part implemented on branch `feat/disconnect-reason` (reason shown for reliable
`disconnect` commands). Still open: wrapping long reasons in the menu, listing the paks that
differ; `path` already prints the search path and loaded pk3 files.

- **Fact:** a client dropped as "Unpure Client" only sees "Server disconnected for unknown reason"
  (found while investigating upstream #659).
- **Shape:** show the real reason (missing or extra pk3, version mismatch, banned) and a command
  that lists loaded pk3 files and their order.

## Server flood protection

Status: checked on 2026-11-09, already present (ioquake3 `SVC_RateLimit` per address and global).
A flood of ~960 queries in 2 s from one address gets 4-11 replies, with fewer bytes out than in;
covered by `QueryFlood` in `tests/test_server.py` (branch `test/network-fuzz`). Spoofed-source
floods are capped by the global bucket. No change made.

- **Shape:** per-IP rate limits for `getstatus`, `getinfo` and connection attempts; audit that
  ioquake3's amplification defenses are present.
- **First step:** check what the code already has.

## Server-side anti-wallhack

Finding (from code): this exists. `sv_netoptimize` (default 0; `g_netoptimize` must stay 1) stops
sending a client the players it cannot see, using line-of-sight traces from the eye position with
lean and a 150 ms velocity look-ahead (`SV_ClientIsVisible` in `code/server/sv_snapshot.c`):
1 checks only players outside the culling distance, 2 checks all of them; results are cached for
`sv_netoptimize_vistime` ms. Remaining work: measure its effect and cost with real clients, and
decide whether to enable it by default. Not verified at run time.

- **Shape:** do not send each client the positions of enemies it cannot see, using map visibility
  (PVS) and line-of-sight traces, so a modified client lacks the data; precedent in Quake III and
  CoD mods.
- **Risk:** CPU cost per snapshot and popping at corners; needs margins and measurement.

## Better bots

- **Fact:** the game module links Recast Navigation for bots (`cmake/basegame.cmake`).
- **Shape:** difficulty levels, team play, objective play (upstream #602), plus reported bugs:
  ladders (#814), grenades (#833), skills and weapons (#684).
- **Why:** keeps low-population servers alive, and bots are the test players for automated tests.
- **Status:** #684 on branch `feat/bot-skill-weapons`: `g_bot_skill` (5 levels scaling the existing
  aim cvars, shown in the browser instead of a fixed "Average"), `g_bot_weapons`. #833 on branch
  `feat/bot-grenades`: bots throw grenades (`g_bot_grenades`), also as their only weapon. Open:
  ladders (#814, Spearhead map), objectives (#602), breaking obstacles with melee.

## Sharp text at high resolutions

- **Fact:** the `USE_FREETYPE` build option exists and is off; MOHAA fonts are bitmaps.
- **Shape:** vector fonts for HUD, menus and console keeping the original style.
- **First step:** check whether FreeType is wired to the game's font code at all.
- **Status:** branch `feat/fonts`: `misc/fonts/make_fonts.py` builds replacement RitualFont + TGA
  pairs from TrueType (DejaVu by default) with the Latin-1 letters, at 2x texture resolution and the
  original widths; CI publishes `zzz-plus-fonts.pk3`. Not replaced: `handle-*` (ornamental),
  `marlett` (symbols). Rendering at the real screen resolution (FreeType at run time) stays open.
- **Checked:** it is not: `USE_FREETYPE` only builds the Quake 3 font code
  (`code/renderercommon/tr_font.c`); MOHAA text uses its own bitmap fonts (`fontheader_t`,
  `code/uilib/uifont.h`). Sharp text means rasterizing a TTF into that structure at the screen
  resolution, with a typeface close to the original and a compatible license.

## Linux packaging: Flatpak and AppImage

- **Shape:** one-click install on any distro and Steam Deck (upstream #890).
- **Status:** AppImage for x86_64 and aarch64 on branch `feat/appimage` (`misc/linux/AppRun`,
  `misc/linux/make-appimage.sh`, built by the Linux CI and published in the `dev` pre-release).
  The AppImage uses the folder it sits in as `fs_basepath` when that folder has `main/`; options
  `--dedicated`, `--spearhead`, `--breakthrough`. Open: Flatpak on Flathub (needs a maintainer
  submission and review), pinning `appimagetool` (CI downloads the `continuous` build).

## Translations

- **Fact:** the engine already loads localization files (`global/localization.txt`).
- **Shape:** make community translations of menus and texts easy (upstream #936 asks for
  Simplified Chinese; Spanish too).
- **Status:** branch `feat/translations`: `cl_language` loads `lang/<code>/*.txt` over the game's
  texts (an extra `localization*.txt` could never translate a text the game already has: first entry
  wins), UTF-8 files converted to Latin-1, missing glyphs drawn as their base letter, HUD labels now
  localized, `localize` command, `misc/i18n/loc_tool.py` (template from the player's own paks,
  checks). No game text in the repository. Open: fonts with accented glyphs, double-byte languages
  (#936).

## Reliable server browser

- **Shape:** fix the hang when a server does not answer (upstream #376); add favorites, history
  and filters (mods, players, ping).
- **Status:** #376 fixed on branch `fix/server-browser-dns`: master DNS lookups were blocking
  (`gethostbyname` on the main thread, 5 s each with a hung DNS) and a failed lookup retried the
  same master forever; also a master socket never created had descriptor 0 and was "closed" on
  timeout, closing stdin. Favorites (key F, `fav_*`), history (`history_*`), `dm_browser_show`,
  `dm_browser_search` and the unused `dm_max_ping` on branch `feat/browser-favorites`; the declared
  game type, pure and sprint filter cvars applied on branch `feat/browser-type-filters`
  (`docs/markdown/03-configuration/06-plus-server-browser.md`).

## Fork identity: versions and updates

- **Fact:** the update checker queries `api.github.com/repos/openmoh/openmohaa/releases/latest`
  (`code/sys/sys_update_checker.cpp`), so fork players are told to "update" to upstream and lose
  the fork's fixes.
- **Shape:** point it at `OpenGamePlus/OpenMOHAA-Plus`; own version scheme (e.g. `0.83.0-plus.1`);
  tagged stable releases with a changelog besides the `dev` pre-release; "Plus" visible in console
  and menu.

## Automated upstream sync

Status: implemented on branch `feat/upstream-sync` (workflow; the schedule must be enabled in the
fork's Actions tab).

- **Shape:** a scheduled workflow merges `upstream/main` into a branch, builds, runs the test
  suite and opens a PR in the fork, so conflicts surface early.

## Mod compatibility tests

Part of the automated test suite. Status: harness on branch `test/mod-compat` (`OMOH_MODS_DIR`);
checked with a working and a broken synthetic mod. Real community mods still have to be collected
(not downloaded by tests).


- **Shape:** load popular mods (Freeze-Tag and others) headless with bots as part of the test
  suite, so fork changes never break what the community runs.

## Server-controlled competitive QoL

- **Shape:** hitsounds, hit markers, round timer on the HUD; off by default and enabled by the
  server, so the Plus client gives no unfair advantage.
- **Status:** hit sounds (`g_hitsounds`, `g_hitsound`) on branch `feat/pause-hitsounds`, played with
  the client's `play` command so original clients hear them. Hit markers (`g_hitmarkers`, client opt-out
  `cg_hitmarker`, sent only to clients that advertise it in their userinfo) on branch
  `feat/hitmarkers`. The round timer was already on the HUD (`hud_timelimit`, "Time Left" from
  `CS_MATCHEND`, round time in round-based modes); it stops during a match pause.

## Plus protocol as a build variant (deferred)

- **Shape:** as OpenCoD2-Plus (`COD2_PLUS_PROTOCOL`, its server's ADR 0015): a CMake option
  defines `OPM_PLUS`; only what changes the network format sits behind it, every other
  improvement stays in both builds. The server keeps announcing protocol 8/17 (listed by GameSpy
  and original clients); the client sends `plus\<version>` in the `connect` userinfo
  (`code/client/cl_main.cpp`), `SV_DirectConnect` refuses other clients with an update notice;
  the Plus client still joins original servers.
- **Status:** not started on purpose. Every current feature works on the original protocol
  (userinfo-gated server commands, `stufftext`, server-only rules), so the option would be an
  empty `#ifdef`. Add it with the first feature that needs a new format (candidates: cgame
  prediction of fork rules, more snapshots or 60 Hz if the current format limits them,
  more entities/configstrings), together with an ADR that scopes the "network formats stay
  compatible" rule to the classic build.
- **Cost:** most players run the original executables; a Plus server shuts them out.

## Master server and community portal (long term)

- **Shape:** a server list that shows each server's Plus features (tick rate, competition mode)
  and leaderboards from match statistics. Only after stats and HTTP exist.

## Translated README and install guides

- **Shape:** README and install instructions in Spanish, Portuguese, German..., as OpenCoD2-Plus
  has in six languages.

## Also discussed

- **Faster server tick:** status on branch `feat/sv-fps-40`: audited; `sv_fps 40` is supported
  (frames are whole milliseconds, so only 20 and 40 run on rate and keep 20 snapshots/s for
  default clients; other values warn). Fixed for any rate: `sv_netoptimize` look-ahead and
  per-frame damping (jeep turning, damage view kick). Decided: weapon fire delays are checked
  once per frame, so at 40 Hz weapons fired closer to their tik values (BAR 400 -> 480 rpm); the
  maintainer chose the 20 Hz feel, so above 20 Hz fire times round to 50 ms steps
  (`G_FireDelayElapsed`, branch `fix/fire-rate-40hz`).
  Map scripts that move things per `waitframe` run twice as fast.
- **Faster server tick (original note):** `sv_fps` above 20 (40 Hz and up) without breaking original clients, as
  OpenCoD2-Plus does.
- **Single binary:** `openmohaa +set dedicated 1` already works as a dedicated server; dropping
  `omohaaded` would need SDL loaded with `dlopen` so headless hosts do not require graphics/audio
  libraries. Recommendation so far: keep both, document `+set dedicated 1`.
- **Saves from the original game** (upstream #601): not listed in Load Game; check whether they can
  be listed and loaded.
