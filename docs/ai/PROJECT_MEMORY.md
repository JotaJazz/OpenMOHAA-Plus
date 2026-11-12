# Project memory

Append-only operational memory. Newest first, at most 3 bullets per stage, never rewrite history.
Template: [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md#memory-entry-template).

## 2026-11-11 — fork licensed AGPL-3.0 (ADR 0006)
- Possible because every GPL header in `code/` is GPL-2.0-or-later; fork-only files (40 sources) now carry AGPL-3.0-or-later, upstream files keep their GPL header, `LICENSE` added next to `COPYING.txt`.
- `vk_core.c` checked against Quake3e's `vk.c` (GPL-2.0, no header): only API boilerplate matches.
- Open: GameSpy SDK has no licence notice (inherited); release zips and the AppImage ship no licence text (pre-existing).

## 2026-11-11 — bug batch: resident cgame, rehashbans line ends, init time
- cgame on emulated musl crashed twice: `CL_ShutdownCGame` runs several times, so "nothing loaded" must not mean "unloaded" (a later call freed the resident module's memory); emitters kept the previous level's tiki pointers and crashed the autosave. `rehashbans` now reads CRLF and a last line without newline; the init time prints positive.
- Not reproduced: client hang on shutdown (kick + quit, SIGTERM). Kept as is: `SetDMMaxAmount`, `G_SetTime` comparison (original-game code, no symptom). The HRTF-off test failed only because the game config picks headphones.
- GitHub Pages enabled (Actions source); the docs workflow deploys. A second Claude session worked in the same checkout meanwhile (stale `index.lock`): use a worktree for parallel sessions.

## 2026-11-11 — README follows the sibling projects; repo moved to OpenGamePlus
- README (en, es) takes the OpenCoD2-Plus / OpenBF1942-Plus layout: banner, "What works today", callouts, Game versions, Roadmap, collapsible Building / Testing / "What is where"; maintainer is OpenGamePlus, as in OpenBF1942-Plus.
- The repo move had broken the update checker (old API URL answers 301, curl does not follow) and silently disabled the upstream-sync workflow (`github.repository` guard).
- Not done: four more README languages (siblings have six), own screenshots (still OpenMoHAA's v0.60.0 ones); the update checker still finds nothing until a stable (non-pre-release) release exists.

## 2026-11-11 — docs audit: drift fixed
- Checker passed but facts had drifted (fork owner now `OpenGamePlus`, lag compensation missing from the summary, incomplete fork-file list, "next" already done, `test_lz77` claimed unwired while CTest runs it in CI): checks against git and code catch what path/link checks cannot.
- Phase progress moved from `ideas.md` to `SYSTEM_SUMMARY.md` (live state in one place); `ideas.md` stays the backlog.
- `AGENTS.md` now allows fork user pages `docs/markdown/**/NN-plus-*.md` (practice already did); doc-sync row added for new cvars, commands and events.

## 2026-11-09 — Vulkan renderer in a window; opengl2 checked
- First windowed run of `renderer_vulkan` on real hardware (NVIDIA RTX 5080, proprietary driver, X11): `training` played, `vid_restart` at first start, clean exit; the picture looked right to the maintainer.
- Headless FPS (`com_speeds`, no vsync): GL1 and Vulkan both hit the engine's 1000 fps cap (integer ms frames) up to 4K; at 8K Vulkan ~580, GL1 still capped. Headless GL may not wait for the GPU, so the gap is likely overstated.
- `renderer_opengl2` built and run for comparison: not usable for MOHAA (terrain, alpha-tested sprites, HUD colors); recorded in `debt.md`.

## 2026-11-09 — bots throw grenades (#833)
- Bots never threw grenades and with grenades only took out an empty pistol (`UseWeaponWithAmmo` skipped throwables). Now: switch, aim above the enemy, charge 0.7-2 s (overcook is at 4 s), throw, switch back.
- Three traps found by tracing: `WEAPON_CLASS_THROWABLE` includes the binoculars (use `WEAPON_CLASS_GRENADE`); `useWeapon()` is undone because `CheckValidWeapon` sees the bot without a weapon mid-switch and re-arms it (skipped during a throw); the switch then works through the player's own `useweaponclass grenade`.

## 2026-11-09 — Vulkan renderer, first stage
- Not a port: the GL1 renderer calls OpenGL through 76 `qgl*` pointers, mostly GL 1.x fixed function, so `renderer_vulkan` is GL1 plus an emulation of those calls (ADR 0005); no renderer copy, pictures comparable pixel by pixel with GL1.
- First bug: front faces must stay counter-clockwise when the shader flips y (clockwise culled the whole BSP while models showed). Spawn points are random, so screenshots use an `sv_scripts` camera.
- Not tested: a real window/swapchain, Windows/macOS builds; Zink cannot run on lavapipe (rejects CPU devices), so Zink was not an option to test here.

## 2026-11-09 — lag compensation (g_antilag)
- Bullets rewind the other players to the shooter's `usercmd.serverTime` (the time the client renders), restoring position, angles and the animation state the bone hit locations are computed from (the per-frame pose cache is reset); history of 64 frames, capped by `g_antilag_maxms`.
- Counting hits was too noisy to test (client pitch jumps with recoil, SMG spread); the test checks the logged sideways aim distance instead: about 2 units from the rewound bot, 14 from where it is now, with 150 ms of client delay.
- Not done: hit validation, projectiles; the kill-while-rewound restore keeps the new animation (seen once, death animation not inspected).

## 2026-11-09 — bot skill levels and weapons (#684)
- Bots already had aim cvars (`g_bot_attack_*`, `g_bot_turn_speed`) and reported a fixed "Average" skill; `g_bot_skill` 1-5 multiplies them (3 = unchanged) and `g_bot_weapons` replaces the automatic weapon choice. The level's real effect on fights is not measured.

## 2026-11-09 — HUD round timer already existed
- `hud_timelimit` shows "Time Left" from `CS_MATCHEND` (round time in round-based modes); it was invisible in tests only because they run without a time limit. Screenshots show it standing still during a match pause.

## 2026-11-09 — hit markers
- Server command `hitmarker` sent only to clients whose userinfo has `cg_hitmarker 1` (a USERINFO cvar of the Plus cgame), so original clients never see an unknown command; drawn as small boxes (no rotated 2D pictures in the cgame API).
- Tested on a plain background (`r_clear 1`, no world or entities) with the player hitting a bot every 0.1 s, so a marker is always visible.

## 2026-11-09 — 4:3 menus on wide screens (#583)
- Smaller than expected: menu containers center themselves (`UIWidgetContainer::AlignPosition`) and children are relative to them, so one scale for both axes in `SetVirtualScale` gives centered 4:3 menus and loading screens; the container's background fill draws the bars. `ui_menuaspect 0` keeps the stretched original.

## 2026-11-09 — browser game type filters
- The original filter cvars (`dm_free_for_all` ... `dm_liberation_match`, `dm_filter_pure`, `dm_run_fast`/`dm_run_normal`) were declared and never applied; now applied client-side from `gametype_i`, `pure` and `sprinton` (masters may not understand filters), favorites exempt.

## 2026-11-09 — fonts with accents (zzz-plus-fonts.pk3)
- Glyph positions are divided by a nominal 256 and screen sizes come from those nominal pixels, so a 2x texture is sharper without changing layout; fonts load without mipmaps, so 4x looks grainy. The TGA must be bottom-up (the top-down flag is ignored).
- DejaVu only (URW fonts are AGPL); glyphs are stretched to the original widths (facfont was 1.3x wider and got clipped in the HUD) and facfont keeps its grey fill with a dark outline.
- A pixel-count test of `n` vs `ñ` was too noisy in a 3D scene (random spawn, brush entities even with `r_drawworld 0`); visual checks were done by screenshots, the test checks loading.

## 2026-11-09 — plus-match competition mod
- Reference mod in scripts only: score per round, halftime `team_swap`, win at rounds/2+1 via `fraglimit`, draw, team timeouts with auto-resume (`registercmd` commands reach scripts from players' consoles with `self` = the player).
- Pitfalls: round-based modes restart the map every round, so `main` reruns (state in cvars; `fraglimit` set in `match_start`, not `main`); `level_intermission` also fires between rounds.
- Not covered by a test: `timeout` typed by a real player (the test uses the server console form).

## 2026-11-09 — 40 Hz keeps the 20 Hz fire rate
- Measured with a temporary trace (now `g_showweaponfire`): BAR 150 ms at 20 Hz, 125 ms at 40 Hz; the maintainer chose the original feel. Above 20 Hz `ReadyToFire` waits for the delay a 20 Hz frame would give; 20 Hz keeps the original expression.
- Semi-automatics are limited by clicks and animations: with 20 ms click spam the shortest interval at 40 Hz (rifle 225 ms, shotgun 825 ms) is not below 20 Hz (200, 800 ms).

## 2026-11-09 — match pause and hit sounds
- Pause reuses `PMF_FROZEN` (as `freezeplayer`) plus no damage; clocks stand still by moving `m_fRoundTime` forward every paused frame and by subtracting the paused time from the time limit (HUD end time included), so resuming needs no catch-up.
- Hit sounds go through `stufftext "play <file>"` (whitelisted in the cgame filter), so original clients hear them; the default file is the stock `sound/items/hit_notify.wav`.
- Script gotcha: a return command without arguments is read as a string in expressions; state is exposed as a level getter (`level.match_paused`). The round-mode clock shift has no test of its own.

## 2026-11-09 — Linux AppImage
- AppImage rather than Flatpak first: no store review, runs on any distro and the Steam Deck; the binaries have no rpath, so `AppRun` sets `LD_LIBRARY_PATH` to the bundled SDL2/OpenAL/cURL.
- Tested here: the unpacked AppDir (server, client, Spearhead launcher) with the game folder taken from the AppImage's location. Not tested here: `appimagetool` itself (downloading and running it locally was refused); CI builds and publishes it.

## 2026-11-09 — translations (cl_language)
- Four upstream gaps stopped translations: the first entry of a text wins (an added `localization_es.txt` was ignored), `UILabel` never localized its text (HUD "Health"), the file-sort comparator compared pointer bytes, and a width loop read the first character every time.
- The game's fonts hold printable ASCII only: accents were drawn as `?`; now as base letters. Language files may be UTF-8 (converted to Latin-1, keys included: two original keys contain Latin-1 bytes).
- No game text is committed: `misc/i18n/loc_tool.py` builds the template from the player's own paks; a full Spanish translation would be a community/user file.

## 2026-11-09 — server browser favorites, history, filters
- Half of it existed: favorites sorted first and drawn as header rows (`m_bFavorite`), master-side filters; nothing stored or set favorites, and `dm_max_ping` and the gametype filter cvars were declared but never applied (gametype filters still are not).
- Favorites and history are queried with `ServerListAuxUpdate`; with no master answering they were never queried (no UDP update sockets, list went idle), and `updatelist` had the same descriptor-0 problem as the master sockets.
- Servers publish `net_gamespy_port` in serverinfo so the history knows the query port; key F is not covered by a test (no key injection headless).

## 2026-11-09 — server browser DNS freeze (#376)
- Reproduced with an `LD_PRELOAD` shim that makes host-name lookups take 5 s (numeric addresses must pass through, or socket setup slows down too): refreshing the list froze the game ~8 s on upstream.
- Three faults in `gserverlist.c`: blocking `gethostbyname`, no move to the next master on DNS failure, master sockets zero-initialized so a never-created one closed descriptor 0 (stdin) on timeout.
- Debugging lesson: a leftover headless client from a failed probe kept running; list `pgrep -x openmohaa` before blaming the code.

## 2026-11-09 — game install detection
- Upstream's Windows store lookups were ioquake3's Quake 3 ones (Steam 2200, GOG 1441704920, Microsoft Store "Quake 3"): GOG now uses MOHAA War Chest 1207659126, the others are off. Windows paths not tested here (no Windows); CI compiles them.
- New `fs_eapath` (EA installer folder, also in the Wine prefix) when the base path has no `main/Pak0.pk3`; the bare "Couldn't load default.cfg" now lists the searched folders and the `fs_basepath` fix.

## 2026-11-09 — HUD scale and crosshair size (#585, #911)
- Up to 1920x1080 the HUD was never scaled (drawn at 640x480 pixel size); `ui_hudscale` multiplies the automatic scale, `getScreenWidth` uses the real scale instead of a fixed 1920.
- Headless screenshot tests: `sv_maxclients 2` avoids the loading screen's continue button, `screenshot silent` avoids the "Wrote" centerprint, `popmenu 0` closes team/weapon menus.
- Not done: menus stretch on wide screens (per-axis virtual scale), sharp text (MOHAA bitmap fonts, FreeType only wired to Quake 3 fonts). FOV is already Hor+.

## 2026-11-09 — server administration (#905)
- In the server module (`sv_admin.cpp`), not the game: `ad_*` wrap existing console commands with their output redirected to the admin, accounts survive map changes. Without libsodium/yaml-cpp: PBKDF2-SHA256 in-tree (checked against Python's `hashlib`), plain-text config (maintainer choice).
- A password check costs ~50 ms of server frame: checked only for existing enabled accounts, at most one per second, per-address throttle; `ad_login` hidden from the `developer 2` command log.
- Pitfalls found: `rconPassword` is only `CVAR_TEMP` (so `ad_set` refuses password cvars without `admin.rcon`), a `"` in a `print` server command cuts the text, level changes must go through `Cbuf`.

## 2026-11-09 — gametype API level 2
- `team_swap` swaps the player lists and scores of both teams in one step (moving players one by one through `JoinTeam` fails on "team is full" with uneven teams) and respawns living players.
- Upstream bug: `team_getscore` was declared `EV_NORMAL`, so scripts could never read its value; now `EV_RETURN`. Most of level 2 already existed (`teamwin`, `team_setscore`, `join_team`, `respawn`).
- Gotcha: in round-based modes the map restarts every round, so `main` runs again; tests guard round-1-only code with the score.

## 2026-11-09 — phases 0-3 and part of 4 merged
- 23 branches merged into `main`: test suite, watchdog, HRTF, disconnect reasons, sanitizer fixes, fuzzing, `sv_perf`, upstream sync, gametype events, libmoh, script toolkit, `sv_fps 40`, `sv_scripts`, plus-stats mod, HTTP downloads, Spanish README.
- Upstream bugs found on the way: libcurl never loaded since the CMake rewrite, `compilescript` aborted the server, HTTP download redirection was dead code; a `RB_Color4f` fix was rejected because the HUD relies on it.
- Open: fire rate rises at 40 Hz (BAR 400 -> 480 rpm; maintainer decision), GitHub Pages and the upstream-sync schedule need enabling, base paks `Pak5`/`pak6` not treated as base paks (debt.md).

## 2026-11-09 — phase 0 started
- `feat/fork-identity`: version `0.83.0-plus.1`, update checker reads the fork's releases and parses `-plus.N` tags; an unparsable tag no longer stores uninitialized values.
- Corrected an earlier note: the `dev` pre-release's Windows `-pdb` zips include the executables; the docs workflow fails only because GitHub Pages is not enabled on the fork.

## 2026-11-09 — idea backlog prioritized
- `docs/dev/roadmap/ideas.md` now holds 36 ideas plus a plan: 11 clusters (overlaps and dependencies) and 6 phases, foundations first, then the script platform the community favors.
- Found while brainstorming: the update checker queries upstream's releases (`code/sys/sys_update_checker.cpp`), so fork players would be sent to upstream; first task of phase 0.

## 2026-11-09 — feature ideas recorded
- Design discussion recorded in `docs/dev/roadmap/ideas.md`: zPAM-like competition mode (C++ match layer + script rules, since MOHAA game modes are C++), libmoh (script functions added natively, NightFall-compatible HTTP first), Vulkan renderer as a new renderer module.
- Checked along the way: `openmohaa +set dedicated 1` already runs as a dedicated server; the fs_homepath fixes made in OpenCoD2-Plus are not needed here (ioquake3 filesystem already handles them).

## 2026-11-09 — README and project direction
- README rewritten after OpenCoD2-Plus: the fork exists to advance with AI agents (forbidden upstream) and will add new features, not only fixes; a faster server tick (40 Hz and up) is the first idea, still unstarted.
- A fresh-context review of the docs found real errors (nonexistent `ttycon` cvar, unbuilt `test_lz77`, wrong function names, missing `corepp` coupling); fixed, and the checker now checks paths against what git would commit.
- The `dev` pre-release ships only `-pdb` archives for Windows and the "Generate Documentation" workflow fails on the fork; both open.

## 2026-11-09 — technical documentation system
- Added `AGENTS.md`, `docs/dev` (architecture, ADRs, development, debt, glossary) and `docs/ai`; `LAYOUT.md` (stale: named the old game and docker directories) now points to the overview.
- Upstream user docs (`docs/markdown`, Doxygen main page `docs/README.md`) left untouched to keep upstream merges clean; docs written in English.
- Added `misc/docs-check` (links, paths, personal data, memory format) with unit tests and its own CI workflow.

## 2026-11-09 — batch merged into main
- Merged 13 fix branches with `--no-ff`; merged `main` passed headless server (map changes, bots) and client (maps, `vid_restart`) smoke tests; branches deleted.
- A false alarm (client ignoring stdin commands) was the intro video with `developer 0`; tests now pass `+set cl_playintro 0`.

## 2026-11-09 — third batch: #287, #918, #850, #422
- #287: same-map restart left the Director timer at the previous map's time, firing all script waits at once; synced in `G_SetTime`. #918: engine freed `TAG_GAME` while a resident module (musl) still used it; reproduced via `LD_PRELOAD` no-op `dlclose`.
- #850 (entity numbers shift with `sv_maxbots`) and #422 (`map` vs `spmap`) documented as working as intended.
- Open: `cgame` has the same unconditional free as #918.

## 2026-11-08 — second batch: #534, #702, #589, #659
- #534 not reproducible; a cursor double-free found nearby was fixed. A first attempt that kept the renderer's free function crashed on `vid_restart` and was reworked.
- #702 `waitForState` now waits (behaviour change vs original, no stock script uses it); #589 `.wav` default extension; #659 stale pak references on pure servers (issue's hang not reproduced).
- User asked that tests never open windows or play sound: offscreen SDL + null OpenAL became the rule.

## 2026-11-08 — first batch: #885, #923, #530, #916, #926, #672, #920
- Each fix reproduced before/after on real binaries (audio measured with the OpenAL wave backend; HUD via screenshots; server via stdin console).
- #885 raises the gain clamp, so the default mix gets louder; noted as a risk rather than reverted.
- Skipped #928/#917 (upstream PRs exist) and #937 (keyboard not testable headless).

## 2026-11-08 — fork created
- Forked `openmoh/openmohaa` as `JotaJazz/OpenMOHAA-Plus`; remotes `origin` (fork) and `upstream`.
- Upstream forbids AI-generated contributions, so all assistant work stays in the fork (ADR 0001).
