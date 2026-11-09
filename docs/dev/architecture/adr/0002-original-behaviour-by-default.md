# 0002. Original-game behaviour by default

- Status: Accepted
- Date: 2026-11-08

## Context
The game logic reimplements MOHAA; mods and maps depend on original quirks. Upstream already marks
deliberate deviations in code (`// Fixed in OPM`, `// Added in OPM`; several hundred occurrences).

## Decision
Match the original game unless the original behaviour is clearly a bug with no content relying on
it. Every deviation is marked with the `... in OPM` markers defined in `CONTRIBUTING.md` and a
one-line reason, and its commit message states the behaviour change. Requests that are
working-as-intended in the original (e.g. `map` vs `spmap`, entity numbers shifting with `sv_maxbots`) are documented, not
changed.

## Consequences
- Fixes such as `waitForState` actually waiting are allowed but must call out the behaviour change.
- Before changing behaviour, check stock content (scripts in the game's pk3 files) for uses.
- Rejected changes and their reason go to `docs/dev/roadmap/debt.md`.
