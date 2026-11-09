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
- CTest registers no project test: `cmake/tests/lz77.cmake` (via `cmake/tests/all.cmake`) is not
  included by the build, so `test_lz77` is never built and CI's `ctest` runs nothing of ours.
- `code/parser/lex.yy.cpp` and `code/parser/y.tab.cpp` are stale committed copies; the build
  generates its own parser.

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
