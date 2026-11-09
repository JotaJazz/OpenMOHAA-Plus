# System summary

Last updated: 2026-11-09. Live snapshot; update when the facts below change.

## Repository

| Item | State |
|---|---|
| Fork | `JotaJazz/OpenMOHAA-Plus` (`origin`), upstream `openmoh/openmohaa` (`upstream`) |
| Default branch | `main`; fix branches are merged with `--no-ff` and deleted |
| Upstream base | `a2f34019` (docs: clarify contribution guidelines) |
| Fork-only files | `AGENTS.md`, `docs/dev/`, `docs/ai/`, `misc/docs-check/`, `.github/workflows/docs-check.yml` |

## Build outputs

`openmohaa`, `omohaaded`, `game.so`, `cgame.so`, `renderer_opengl1.so`, `launch_openmohaa_*`,
`test_lz77`. Details: [overview.md](../dev/architecture/overview.md).

## Subsystem state (fork changes on top of upstream)

| Subsystem | Fork change | Upstream issue |
|---|---|---|
| Client audio (OpenAL) | Source gain above 1.0 allowed (`AL_MAX_GAIN`) | #885 |
| Client audio | Extensionless sound names default to `.wav` | #589 |
| Frame loop (`qcommon`) | `com_maxfps` remainder carried across frames | #530 |
| Server / savegames | svs time fixups add instead of overwrite (stopwatch after load) | #916 |
| Server bans | `rehashbans` skips blank lines and stale slots | #926 |
| Server / modules | `TAG_GAME` freed only if `game.so` really unloaded; event tables kept | #918 |
| Filesystem / pure | Paks rejected by a pure server not reported as referenced | #659 |
| Script VM | Director time synced in `G_SetTime` (no burst after same-map restart) | #287 |
| Script commands | `registercmd` dispatched; item `amount`/`dmamount`... readable; `waitForState` waits | #672, #920, #702 |
| SDL input | Cursor pointers cleared, cursor image freed immediately | (found via #534) |
| CI | Executable bits restored before release zips; docs checker workflow | #923 |

## Tests

| Kind | Count / command |
|---|---|
| CTest | 1 (`test_lz77`) |
| Docs checker | `python3 -I misc/docs-check/check_docs.py`, unit tests in `misc/docs-check/test_check_docs.py` |
| Manual headless recipes | [testing.md](../dev/development/testing.md) |

## Open work

See [debt.md](../dev/roadmap/debt.md). Next candidates: musl-safe `cgame` unload, remaining
testable upstream issues.
