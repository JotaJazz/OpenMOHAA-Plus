# Architecture notes (normative summary)

Full rules: [boundaries.md](../dev/architecture/boundaries.md). Decisions: [ADR index](../dev/architecture/adr/README.md).

## Must
- Modules (`fgame`, `cgame`, renderers) reach the engine only via `gi` / `cgi` / `ri`.
- Bump the matching API version constant when an import/export table changes.
- Keep savegame and network formats compatible with the original game.
- Mark deviations from original behaviour with the `... in OPM` markers from `CONTRIBUTING.md` + reason.
- Assume a module may stay resident after unload: no engine bulk-free of its memory unless it is
  really gone; init code tolerates surviving globals.
- Readable script properties need an `EV_GETTER` event; anything holding scripts/strings across
  maps is cleared in `ScriptMaster::Reset`.

## Must not
- Include engine-private headers (`code/server`, `code/client`, `code/sys`) from a module.
- Hand-edit parser output (generated at build time from `code/parser/bison_source.txt` /
  `code/parser/lex_source.txt`) or vendored `code/thirdparty` / `code/gamespy` sources.
- Keep function pointers from the renderer across `vid_restart`.
- Address map entities by number in scripts or tests when bots may be enabled.
- Submit fork commits or assistant-written text upstream.

## Irreversible / costly to change
- Savegame layout (`code/fgame/archive.cpp`): old saves must keep loading.
- Module ABI tables (`code/fgame/g_public.h`, `code/cgame/cg_public.h`, `code/renderercommon/tr_public.h`).
- Original-behaviour baseline expected by mods and maps.

## ADRs
0001 AI-assisted work stays in the fork · 0002 original behaviour by default · 0003 one branch per
issue, `--no-ff` merges · 0004 module may stay resident after unload · 0005 the Vulkan renderer
emulates the GL1 renderer's OpenGL calls · 0006 the fork is distributed under AGPL-3.0.
