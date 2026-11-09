# Project memory

Append-only operational memory. Newest first, at most 3 bullets per stage, never rewrite history.
Template: [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md#memory-entry-template).

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
