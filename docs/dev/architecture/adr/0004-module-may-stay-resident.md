# 0004. A game module may stay resident after unload

- Status: Accepted
- Date: 2026-11-09

## Context
On musl `dlclose()` never unloads a library, so `game.so` keeps its globals across map changes.
The engine freed all `TAG_GAME` memory after unloading, leaving those globals dangling
(upstream issue #918). glibc and Windows normally unload, hiding the bug.

## Decision
`Sys_UnloadGame` reports whether the library is really gone (`dladdr` on `GetGameAPI`), and
`SV_ShutdownGameProgs` frees `TAG_GAME` only in that case. `L_ShutdownEvents` no longer clears the
event tables, so a resident module can reuse them.

## Consequences
- Module init code must tolerate surviving globals.
- Known follow-ups are tracked in `docs/dev/roadmap/debt.md`, not here.
- Test by emulating musl: `LD_PRELOAD` a `dlclose()` that does nothing (see testing guide).
