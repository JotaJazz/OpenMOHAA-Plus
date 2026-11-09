# Project context

## Identity
OpenMOHAA Plus: a fork of OpenMoHAA (`openmoh/openmohaa`), the open reimplementation of
Medal of Honor: Allied Assault, Spearhead and Breakthrough (client, dedicated server, game logic)
on an ioquake3-based engine. Part of the maintainer's "Plus" project family.

## Goal
Fix bugs reported upstream (and others found along the way) in a fork that can keep merging
upstream, with every fix reproduced and verified on real binaries.

## Non-goals
- Submitting AI-assisted work upstream (forbidden by upstream policy; ADR 0001).
- Rewriting or modernizing subsystems; large refactors.
- Shipping game data (pk3 files are proprietary and never committed).
- Changing original-game behaviour without a clear bug (ADR 0002).

## Principles
- Root cause over symptoms; smallest complete change.
- Reproduce before and after with the same command; report what was not checked.
- Upstream files stay untouched where possible; fork material lives in new files.
- Tests and tools never open windows or play sound on the shared machine.

## Stack
C (engine) and C++ (game, UI, script VM, corepp); CMake + Ninja; SDL2; OpenAL Soft; GameSpy SDK;
flex/bison for the script grammar; GitHub Actions; Doxygen for user docs; Python (stdlib) for
the docs checker.

## Constraints
- Must stay compatible with original game data, saves and network protocol.
- Module ABI versions (`GAME_API_VERSION`, `CGAME_IMPORT_API_VERSION`, `REF_API_VERSION`).
- Development machine: Linux under WSL with only Allied Assault data; no Spearhead/Breakthrough,
  macOS or GPU-specific testing.
- Docs, code comments and commits in English.
