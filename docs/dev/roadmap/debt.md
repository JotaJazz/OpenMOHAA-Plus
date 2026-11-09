# Debt, risks and deferred decisions

Update when a fix leaves something open or an issue is rejected. Issue numbers are upstream
(`openmoh/openmohaa`).

## Known risks introduced by fork fixes

| Item | Risk | Source |
|---|---|---|
| `AL_MAX_GAIN` raised | Default mix ~15% louder (stock aliases at volume 1.5 no longer clamp); peaks above full scale clip because the OpenAL limiter is disabled | #885 |
| `waitForState` now waits | Mods that relied on it returning immediately now block | #702 |
| Resident game module | `cgame` still frees `TAG_CGAME` unconditionally after unload; may crash on musl like #918 did | #918 |
| `sv_pure` stale references | Fix covers stale pak references from the menu; the issue's literal symptom (hang) was never reproduced | #659 |
| `rehashbans` | Last line without trailing newline is still dropped; CRLF leaves `\r` in ban reasons | #926 |

## Known gaps (not fixed)

- `Client Initialization Complete` prints a negative time (`start - end` in `code/client/cl_main.cpp`).
- The client can hang on shutdown after a SIGTERM or after being dropped by a server.
- After an "Unpure Client" drop the client shows "Server disconnected for unknown reason".
- `Item::SetDMMaxAmount` sets the amount instead of the maximum (may be original behaviour).
- `G_SetTime` compares `level.svsStartTime != svsTime` (likely meant `svsStartTime`).
- Stopwatch type (fuse/wet fuse) is not saved; only matters for Spearhead/Breakthrough.
- No `clang-format` run on fork commits so far.
- The "Generate Documentation" workflow (`.github/workflows/doxygen-publish.yml`) fails on the fork
  because GitHub Pages is not enabled for the repository (deploy returns 404). Fix: enable Pages
  with source "GitHub Actions" in the repository settings (maintainer decision; it publishes the
  user documentation site). The `dev` pre-release's Windows `-pdb` zips are complete packages
  (executables plus debug symbols), so they are playable.
- `code/parser/lex.yy.cpp` and `code/parser/y.tab.cpp` are stale committed copies; the build
  generates its own parser.

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
