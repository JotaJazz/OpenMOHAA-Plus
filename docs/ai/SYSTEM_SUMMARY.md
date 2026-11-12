# System summary

Last updated: 2026-11-11. Live snapshot; update when the facts below change.

## Repository

| Item | State |
|---|---|
| Fork | `OpenGamePlus/OpenMOHAA-Plus` (`origin`), upstream `openmoh/openmohaa` (`upstream`) |
| Default branch | `main`; work branches are merged with `--no-ff` and deleted |
| User docs site | GitHub Pages from `.github/workflows/doxygen-publish.yml` (source "GitHub Actions", on push to `main`) |
| Upstream base | `a2f34019` (docs: clarify contribution guidelines) |
| Fork-only files | `AGENTS.md`, `README.es.md`, `docs/banner.svg`, `docs/dev/`, `docs/ai/`, `tests/`, `misc/docs-check/`, `misc/scr-tools/`, `misc/vscode-mohaa/`, `misc/mods/`, `misc/fonts/`, `misc/i18n/`, `misc/linux/`, `code/renderervk/`, `cmake/renderer_vulkan.cmake`, `code/fgame/g_antilag.*`, `docs/markdown/**/*plus-*.md`, `.github/workflows/docs-check.yml`, `.github/workflows/upstream-sync.yml` |
| Upstream files modified (merge conflict risk) | `LAYOUT.md` (pointer), `.gitignore`, `README.md`, `docs/markdown/04-coding/02-coding.md`, `.github/workflows/{branches-build,shared-build-linux,tags-publish-release}.yml`, the files touched by the fixes below |

## Build outputs

`openmohaa`, `omohaaded`, `game.so`, `cgame.so`, `renderer_opengl1.so`, `renderer_vulkan.so` (when the
Vulkan headers are found), `launch_openmohaa_*`.
Details: [overview.md](../dev/architecture/overview.md).

## Subsystem state (fork changes on top of upstream)

| Subsystem | Fork change | Upstream issue |
|---|---|---|
| Client audio (OpenAL) | Source gain above 1.0 allowed (`AL_MAX_GAIN`) | #885 |
| Client audio | Extensionless sound names default to `.wav` | #589 |
| Client audio | `s_hrtf 1` forces HRTF; HRTF status logged | (phase 0) |
| Client connection | Reason of a mid-connection drop shown in console and error menu | (phase 0) |
| Frame loop (`qcommon`) | `com_maxfps` remainder carried across frames | #530 |
| Server / savegames | svs time fixups add instead of overwrite (stopwatch after load) | #916 |
| Server bans | `rehashbans` skips blank lines and stale slots, reads CRLF files and a last line without newline | #926 |
| Server / modules | `TAG_GAME` freed only if `game.so` really unloaded; event tables kept; same for `cgame.so` (`TAG_CGAME`), whose emitters are dropped at shutdown | #918 |
| Filesystem / pure | Paks rejected by a pure server not reported as referenced | #659 |
| Script VM | Director time synced in `G_SetTime` (no burst after same-map restart) | #287 |
| Script commands | `registercmd` dispatched; item `amount`/`dmamount`... readable; `waitForState` waits | #672, #920, #702 |
| SDL input | Cursor pointers cleared, cursor image freed immediately | (found via #534) |
| CI | Executable bits restored before release zips | #923 |
| Diagnostics | `com_freezeWatch` freeze watchdog (`code/sys/sys_watchdog.cpp`), crash reports to `crash.log`, fixed Unix backtrace | (phase 0) |
| Profiling | `sv_perf` server frame timings, `Sys_Microseconds` | (phase 1) |
| Identity | Version `0.83.0-plus.1`; update checker reads the fork's releases (`-plus.N` tags) | (phase 0) |
| Client audio | Sound callback reads the right entity slot (ASan out-of-bounds read) | (phase 1) |
| Script VM | Unsigned hash arithmetic (UBSan) | (phase 1) |
| Build | `HAS_LIBCURL` defined again, so libcurl loads (update checker, libmoh, downloads) | (phase 2) |
| Game API | `GAME_API_VERSION 1016` (`gi.HTTP_*`); savegames use `GAME_ARCHIVE_VERSION 15` | (phase 2) |
| Script events | `player_damage`, `player_teamchange`, `match_start`, `round_start`, `round_end` | (phase 2) |
| Script commands | `team_swap` (halftime side swap, `DM_Manager::SwapTeams`); `team_getscore` is a return event | (phase 2) |
| Script functions | libmoh: `create_api_request`, `json_parse`, `json_stringify` (`code/fgame/libmoh.cpp`, `code/server/sv_http.cpp`) | (phase 2) |
| Server commands | `compilescript` reports parse errors instead of aborting | (phase 2) |
| Server tick | `sv_fps 40` supported, warning for unsupported rates; netoptimize look-ahead and damping per frame time; weapons keep the 20 Hz fire rate (`G_FireDelayElapsed`), `g_showweaponfire` | (phase 3) |
| Server scripts | `sv_scripts` runs listed scripts on every level | (phase 3) |
| Filesystem | GOG lookup uses MOHAA's ID (was Quake 3), Steam/Microsoft Store lookups off, `fs_eapath` (EA folder, also in Wine), clear error when the game data is missing | (phase 4) |
| Server browser | Master lookups in the background (`code/gamespy/gs_resolve.cpp`), next master on DNS failure, unused master sockets start as `INVALID_SOCKET` | #376 |
| Server browser | Favorites and history (`code/client/cl_serverbook.cpp`), queried directly on refresh; `dm_browser_show`, `dm_browser_search`, `dm_max_ping`, game type / pure / sprint filters; servers publish `net_gamespy_port` | (phase 4) |
| Fonts | `misc/fonts/make_fonts.py`: DejaVu-based RitualFont replacements with Latin-1, 2x textures, original widths; `zzz-plus-fonts.pk3` built in CI | (phase 4) |
| Localization | `cl_language` overlay (`lang/<code>/`), UTF-8 to Latin-1, `localize` command, labels localized, missing Latin-1 glyphs drawn as base letters (`code/renderercommon/tr_fontfold.h`), sorted localization files | (phase 4) |
| Packaging | Linux AppImage (x86_64, aarch64) built in CI, `misc/linux/AppRun` picks the game folder | #890 |
| Competition mod | `misc/mods/plus-match` (scripts only): rounds, halftime side swap, draw, team timeouts | (phase 3) |
| Netcode | Lag compensation for bullets (`g_antilag`, `g_antilag_maxms`, `code/fgame/g_antilag.cpp`): other players rewound to the shooter's `usercmd.serverTime` | (phase 3) |
| Bots | `g_bot_skill` levels (scale reaction, spread, turn speed), `g_bot_weapons`, grenade throws (`g_bot_grenades`) | #684, #833 |
| Match control | Match pause (`DM_Manager::SetPaused`, `matchpause`, `match_pause`, `ad_pause`), hit sounds (`g_hitsounds`), hit markers (`g_hitmarkers`, `cg_hitmarker`) | (phase 3) |
| HUD | `ui_hudscale` multiplier; `ui_menuaspect` (4:3 menus with side bars); `cg_crosshair_size`/`cg_crosshair_scale`; UI screen width from the real scale | #585, #911 |
| Administration | Admin accounts, roles, allow/deny permissions, `ad_*` commands, audit log (`code/server/sv_admin.cpp`) | #905 |
| Downloads | HTTP download redirection (`sv_dlURL`): `USE_CURL` defined, `cl_curl.c` fixed, server info parsed | (phase 4) |
| Renderer | `renderer_vulkan` (`cl_renderer vulkan`): GL1 renderer on a Vulkan emulation of its GL calls (`code/renderervk`, `USE_VULKAN` in `code/sdl/sdl_glimp.c`); headless offscreen mode; Linux CI builds it | (phase 5) |
| Docs | Technical docs (`docs/dev`, `docs/ai`), docs checker and its workflow | - |

## Tests

| Kind | Count / command |
|---|---|
| CTest | 1 (`test_lz77`) |
| Game regression tests | `tests/` (server and headless client, need game data; `OMOH_SLOW=1` for the slow ones); every class skips without game data |
| Script tools | `python3 -I misc/scr-tools/test_scr_tools.py` |
| Docs checker | `python3 -I misc/docs-check/check_docs.py`, unit tests in `misc/docs-check/test_check_docs.py` |
| Manual headless recipes | [testing.md](../dev/development/testing.md) |

## Open work

Phase progress of the [prioritized plan](../dev/roadmap/ideas.md#prioritized-plan):

| Phase | Done | Open |
|---|---|---|
| 0 | all | - |
| 1 | all (flood protection already existed) | - |
| 2 | gametype API levels 1 and 2 (`team_swap`, match pause), libmoh MVP, script toolkit | - |
| 3 | `sv_fps 40` (20 Hz fire rate kept), `sv_scripts`, plus-stats and plus-match reference mods, server administration (#905), match pause, hit sounds, hit markers, bot skill, weapons and grenades, lag compensation; ready-up, player info, anti-wallhack, votes and the HUD round timer already existed | bot objectives (#602), hit validation |
| 4 | HTTP mod download, Spanish README, server browser, install detection, widescreen + sharp text, AppImage, in-game translations | Flatpak, demos, #601 saves (needs an original save file) |
| 5 | Vulkan renderer, first stage | rest of Vulkan, campaign co-op, WebAssembly, single binary, community portal, gametype API level 3 |

Next: the open items above; upstream bug fixing continues in parallel. Risks and gaps:
[debt.md](../dev/roadmap/debt.md).
