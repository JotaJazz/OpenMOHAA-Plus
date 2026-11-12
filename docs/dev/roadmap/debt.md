# Debt, risks and deferred decisions

Update when a fix leaves something open or an issue is rejected. Issue numbers are upstream
(`openmoh/openmohaa`).

## Known risks introduced by fork fixes

| Item | Risk | Source |
|---|---|---|
| `AL_MAX_GAIN` raised | Default mix ~15% louder (stock aliases at volume 1.5 no longer clamp); peaks above full scale clip because the OpenAL limiter is disabled | #885 |
| `waitForState` now waits | Mods that relied on it returning immediately now block | #702 |
| Resident cgame module | Tested with an emulated musl (`dlclose()` no-op) over three map loads and the autosave; other cgame globals (scoreboard, `cg` state) still survive and were not audited one by one | #918 |
| Launcher links in `<prefix>/bin` | `omohaaded` is not linked: the engine takes its folder from `argv[0]` (no symlink resolution) and would look for `game.so` in `bin`; run it from `<prefix>/lib/openmohaa` | #526 |
| Configstrings past `MAX_GAMESTATE_CHARS` refused | The server keeps clients by not registering the string that does not fit (a warning names it): that sound or model is missing for everyone. The real cause in the report (a full server with bots named "0", likely mods) was not found; stock maps with 8 bots stay near 20 KB of 41952 | #793 |
| `sv_pure` stale references | Fix covers stale pak references from the menu; the issue's literal symptom (hang) was never reproduced | #659 |

## Known gaps (not fixed)

- The 1.11 `errormessage` menu shows `com_errorMessage` on one centered line, so long reasons (e.g.
  the "Unpure Client ..." text) are clipped on screen; the console has the full text.
- "Unknown client game command: disconnect" is still printed by cgame for mid-connection drops
  (harmless).
- Stopwatch type (fuse/wet fuse) is not saved; only matters for Spearhead/Breakthrough.
- No `clang-format` run on fork commits so far.
- Admin system (#905): `admin_config.txt` is read at start only (no reload command); `ad_*` cover server console commands only, not game commands (for example `callvote` settings); login throttling is per address and kept in memory.
- The #885 gain fix has no automated test yet (needs time-aligned audio measurement with the OpenAL
  wave backend; manual method in `docs/dev/development/testing.md`).
- Base paks: the client skips only `pak0`-`pak4` when comparing paks (`NUM_ID_PAKS 5` in
  `code/qcommon/qcommon.h`) and the server's id-pak guard is commented out in
  `code/server/sv_client.c`, while MOHAA ships `Pak0`-`pak6`: a client with a different Pak5/pak6
  could try to download it. Not tested.
- `code/parser/lex.yy.cpp` and `code/parser/y.tab.cpp` are stale committed copies; the build
  generates its own parser.

- Vulkan renderer ([renderer-vulkan.md](../architecture/renderer-vulkan.md)): one frame in flight
  (every submit waits for the GPU), one memory allocation per texture (drivers may cap allocations
  near 4096), pipelines created on first use (a hitch the first time a state appears). The window
  and swapchain path and `vid_restart` were tested only on NVIDIA (proprietary driver, X11, one
  session on `training`); on Windows only an offscreen run (MSVC x64, NVIDIA, `dm/mohdm2`), no
  window or swapchain yet; AMD/Intel, Wayland and the Windows x86/ARM64 builds are not tested.
  macOS builds without it (its CI keeps `USE_RENDERER_DLOPEN=OFF` and has no MoltenVK).
- `renderer_opengl2` (`BUILD_RENDERER_GL2=ON`, off by default) loads and runs but is not usable for
  MOHAA. Seen on `training` (NVIDIA RTX 5080, Linux, headless and windowed, 2026-11-09): terrain is
  not drawn (black holes beside the road), alpha-tested tree sprites draw as solid rectangles, the
  health bar is red and the compass faint (likely the `RB_Color4f` byte scaling below, which GL1
  depends on), sky and lighting are greyer than GL1. Causes not investigated; no other map tested.

## Sanitizer findings (headless suite on an ASan + UBSan build, 2026-11-09)

Fixed: out-of-bounds read in `S_OPENAL_Start2DSound` (branch `fix/snd-callback-entity`), signed
overflow in the event and string hash functions (branch `fix/ubsan-cleanups`). Still reported by
UBSan, not fixed:

- Misaligned loads of packed model data (`code/tiki/tiki_skel.cpp`, `code/renderergl1/tr_model.cpp`,
  `code/renderergl1/tr_image.c`) and of GameSpy query data: harmless on x86, a risk on strict
  alignment CPUs.
- vptr checks: `World`, `Actor`, `ScriptEvent`, `UIWindowManager` objects accessed through pointers
  that UBSan does not consider of that type (`code/fgame/worldspawn.cpp`, `code/fgame/g_main.cpp`,
  `code/uilib/uiwinman.cpp`); needs investigation (static objects across modules or real type
  confusion).
- `RB_Color4f` gets 0-255 bytes from `code/renderergl1/tr_draw.c` and multiplies them by 255 again
  (float to `unsigned char` overflow). Passing the bytes through unchanged turns the health bar red
  and removes the compass background, so the HUD currently depends on this behaviour; the callers'
  intended color values must be worked out before changing it.
- `qsort` with a NULL base and 0 elements in `FS_Startup`; libmad index computations (vendored).

Recipe: Debug build (LTO off) with `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` in
C/CXX flags and `-fsanitize=address,undefined` in linker flags, in a scratch directory; link
`omohaaded`/`openmohaa` to the `-dbg` binaries and run the suite with `OMOH_BIN_DIR` and
`ASAN_OPTIONS=detect_leaks=0`.

## Rejected or not reproducible (with reason)

| Issue | Decision | Reason |
|---|---|---|
| #850 | Not a bug | Entity numbers shift by `sv_maxbots` (client slots come first); scripts must use `targetname` |
| #422 | Not a bug | `map` keeps `g_gametype`; use `spmap`/`spdevmap` (what the menu does); forcing it breaks co-op mods |
| #534 | Not reproducible | `vid_restart` works on current code; report looks NVIDIA-proprietary/X11 specific |
| #937 | Not testable here | Keyboard input cannot be exercised in the headless setup |
| #928, #917 | Skipped | Upstream PRs #933 and #934 already address them |
| #688 | Not reproducible | 2026-11-12: idle 45 s after the briefing on m3l3, m3l2, m1l2a, no statistics screen; it only opens on `+statistics` (unbound by default) or at mission end, so likely a stuck key or controller on the reporter's machine (no real input here) |
| #927 | Not reproducible | 2026-11-12: the tank's two emitters spawned 6 times in `training` with `cg_effectdetail 0.2` and `com_freezeWatch 5`, no freeze; the emitter loops scaled by that cvar terminate; the reporter fixed it by raising every advanced option without isolating which |
| #675, #894 | Not reproducible with AA | 2026-11-12: Allied and German grenades visible in first person (local game and dedicated server, after switching away and back); #894 is Spearhead (`com_target_game 1`), no data here |
| #538 | Not reproduced (macOS) | 2026-11-12: grenade explosion emitters in `training` captured every 0.1 s show the fire with `cg_shadows 0` and `2` alike (Linux, GL1, Mesa); both confirmations are Apple M1 |
| #243 | Not reproducible | 2026-11-12: `thompson_pain_kneestodeath` exists for the winter soldiers through `new_generic_human.tik`; crouching SMG actors on m6l2b killed by script die without the error; reported on a February 2024 build |
| Client hang on shutdown | Not reproducible | 2026-11-11: `quit` after a kick (3 runs) exits in 0.2 s with 0; SIGTERM exits in 0.1 s with 1 (headless, glibc) |
| `Item::SetDMMaxAmount` sets the amount | Kept | Reimplemented from the original game; changing it alters DM ammo amounts, and the original `gamex86.dll` was not compared |
| `G_SetTime` compares `level.svsStartTime != svsTime` | Kept | Only rewrites `CS_LEVEL_START_TIME` more often than needed; no visible symptom, and it sits on the #916 stopwatch path |

## Not attempted (needs data or hardware not available)

Spearhead/Breakthrough-only issues, macOS-only issues, GPU/driver-specific rendering issues, and
large feature requests (HUD scale, 3D menus, ACL admin system, localization).
