# Development guide

Commands: [compiling.md](../dev/development/compiling.md), [testing.md](../dev/development/testing.md).
Git and commits: [fork-workflow.md](../dev/development/fork-workflow.md).

## Workflow
1. Read the upstream issue and comments; check for an existing upstream PR.
2. Find the root cause in code; grep every caller of the function you touch.
3. Reproduce headless ([testing.md](../dev/development/testing.md)); keep the exact command, or
   state why it cannot be reproduced.
4. Smallest complete fix, matching surrounding style; mark original-behaviour deviations.
5. Re-run the same command; also run a nearby regression check.
6. Commit with the verification evidence; docs that the change makes wrong are updated in the
   same commit (table below).
7. After merging a batch into `main`, update `SYSTEM_SUMMARY.md` and add one `PROJECT_MEMORY.md`
   stage on `main` (not on fix branches, to avoid merge conflicts); put leftovers in
   `docs/dev/roadmap/debt.md`.

## Quality bar
- Builds with no new errors; touched behaviour verified before/after on real binaries.
- `python3 -I misc/docs-check/check_docs.py` reports 0 problems.
- Commit message says what was not checked.

## Doc-sync: when you change X, update Y

| Change | Update |
|---|---|
| New/removed build output, CMake option, directory under `code/` | `docs/dev/architecture/overview.md`, `SYSTEM_SUMMARY.md` |
| New cross-directory include or module interface change | `docs/dev/architecture/boundaries.md`, maybe an ADR |
| Architectural or policy decision | New ADR + index in `docs/dev/architecture/adr/README.md`, `ARCHITECTURE_NOTES.md` |
| Batch of fixes merged into `main` | `SYSTEM_SUMMARY.md` subsystem table, `PROJECT_MEMORY.md` stage (on `main`) |
| Fix leaves a risk/gap, or an issue is rejected | `docs/dev/roadmap/debt.md` |
| New test technique or tool | `docs/dev/development/testing.md` |
| Upstream merge | `SYSTEM_SUMMARY.md` (upstream base), conflicts noted in `PROJECT_MEMORY.md` |
| New term used across docs | `docs/dev/glossary.md` |
| New player/server-facing cvar, command or script event | Its `docs/markdown/**/NN-plus-*.md` page, `README.md` and `README.es.md` lists |

## Recipes
- Script-visible property: add an `EV_GETTER` `Event` with the same name + handler, bind it in the
  class `ResponseDef` (pattern: `max_health`, `code/fgame/entity.cpp`).
- Test game logic: map-script override in `<HOME_DIR>/main/maps/`, `developer 1`, bots for players.
- Behaviour that needs a client: offscreen client recipe in testing.md; `screenshotJPEG` to see.

## Memory entry template

```markdown
## YYYY-MM-DD — topic
- What changed and why it mattered.
- What was rejected or deferred, and why.
- What is left open (link to debt.md if long).
```

Newest stage first, at most 3 bullets, never rewrite past stages (add a new one to correct).
