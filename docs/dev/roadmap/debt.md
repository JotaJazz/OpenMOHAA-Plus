# Debt, risks and deferred decisions

Update when a fix leaves something open or an issue is rejected. Issue numbers are upstream
(`openmoh/openmohaa`).

## Known risks introduced by fork fixes

| Item | Risk | Source |
|---|---|---|
| `AL_MAX_GAIN` raised | Default mix ~15% louder (stock aliases at volume 1.5 no longer clamp); peaks above full scale clip because the OpenAL limiter is disabled | #885 |
| `waitForState` now waits | Mods that relied on it returning immediately now block | #702 |
| Resident cgame module | Tested with an emulated musl (`dlclose()` no-op) over three map loads and the autosave; other cgame globals (scoreboard, `cg` state) still survive and were not audited one by one | #918 |
| `sv_pure` stale references | Fix covers stale pak references from the menu; the issue's literal symptom (hang) was never reproduced | #659 |

## Known gaps (not fixed)

- The client can hang on shutdown after a SIGTERM or after being dropped by a server.
- The 1.11 `errormessage` menu shows `com_errorMessage` on one centered line, so long reasons (e.g.
  the "Unpure Client ..." text) are clipped on screen; the console has the full text.
- "Unknown client game command: disconnect" is still printed by cgame for mid-connection drops
  (harmless).
- `Item::SetDMMaxAmount` sets the amount instead of the maximum (may be original behaviour).
- `G_SetTime` compares `level.svsStartTime != svsTime` (likely meant `svsStartTime`).
- Stopwatch type (fuse/wet fuse) is not saved; only matters for Spearhead/Breakthrough.
- No `clang-format` run on fork commits so far.
- Admin system (#905): `admin_config.txt` is read at start only (no reload command); `ad_*` cover server console commands only, not game commands (for example `callvote` settings); login throttling is per address and kept in memory.
- The "Generate Documentation" workflow (`.github/workflows/doxygen-publish.yml`) fails on the fork
  because GitHub Pages is not enabled for the repository (deploy returns 404). Fix: enable Pages
  with source "GitHub Actions" in the repository settings (maintainer decision; it publishes the
  user documentation site). The `dev` pre-release's Windows `-pdb` zips are complete packages
  (executables plus debug symbols), so they are playable.
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
  session on `training`); AMD/Intel, Wayland, Windows and macOS builds (CI installs the Vulkan
  headers on Linux only) are not tested.
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

## Not attempted (needs data or hardware not available)

Spearhead/Breakthrough-only issues, macOS-only issues, GPU/driver-specific rendering issues, and
large feature requests (HUD scale, 3D menus, ACL admin system, localization).
