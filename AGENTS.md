# AGENTS.md

OpenMOHAA Plus: fork of OpenMoHAA (Medal of Honor: Allied Assault engine and game logic).

## Read first
1. [docs/ai/README_AI.md](docs/ai/README_AI.md), then the files it lists, in order.
2. Before touching a subsystem: [docs/dev/architecture/boundaries.md](docs/dev/architecture/boundaries.md)
   and the matching note in [docs/dev/README.md](docs/dev/README.md).

## Hard rules
- Never submit work, PRs, issues or comments to `openmoh/openmohaa` (upstream forbids AI-generated
  contributions). Work stays in this fork. ([ADR 0001](docs/dev/architecture/adr/0001-fork-keeps-ai-assisted-work.md))
- Never open a visible window or play sound when running the game ([testing.md](docs/dev/development/testing.md)).
- Reproduce a bug before fixing it (or state why it cannot be) and verify after with the same
  command; state what was not checked.
- Keep original-game behaviour unless clearly a bug; mark deviations with the `... in OPM` markers
  defined in `CONTRIBUTING.md`.
- Do not edit upstream user docs (`docs/markdown`, `docs/README.md`), vendored code or generated
  parser sources. Fork user docs are new `NN-plus-*.md` pages in `docs/markdown`, linked from
  the upstream index page with as small an edit as possible.
- New fork files are AGPL-3.0-or-later; upstream files keep their GPL-2.0-or-later header
  ([ADR 0006](docs/dev/architecture/adr/0006-fork-licensed-agpl-3.md)). No GPL-2.0-only code.
- Never commit game data (pk3), personal paths or credentials.
- Docs, comments and commit messages in English.

## Doc-sync
- A change that makes a doc wrong updates it in the same commit (table in
  [DEVELOPMENT_GUIDE.md](docs/ai/DEVELOPMENT_GUIDE.md)).
- Finished work adds a stage to [PROJECT_MEMORY.md](docs/ai/PROJECT_MEMORY.md).
- Run `python3 -I misc/docs-check/check_docs.py` before committing docs.
