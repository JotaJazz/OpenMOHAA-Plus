# Project memory

Append-only operational memory. Newest first, at most 3 bullets per stage, never rewrite history.
Template: [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md#memory-entry-template).

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
