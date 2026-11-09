# Glossary

| Term | Meaning |
|---|---|
| AA / SH / BT | Allied Assault (`main/`), Spearhead (`mainta/`), Breakthrough (`maintt/`) |
| OPM | OpenMoHAA (upstream); `// Fixed in OPM` marks deviations from the original game |
| fgame | Server game module (`game.so`), code in `code/fgame` |
| cgame | Client game module (`cgame.so`), code in `code/cgame` |
| `gi` / `ge` | Game import table (engine functions the module calls) / game export table (module functions the engine calls) |
| `cgi` / `cge`, `ri` / `re` | Same pattern for cgame and the renderer |
| `sv` / `svs` | Per-map server state / persistent server state (`svs.time` survives map changes) |
| Director | Global `ScriptMaster` instance: compiled scripts, string table, script thread timer |
| Event | Named command or property in the class/listener system (`code/corepp/listener.h`) |
| Getter / setter | `EV_GETTER` / `EV_SETTER` events behind `ent.field` reads and writes in scripts |
| Listener | Base class that receives events; script threads wait on listeners |
| `.scr` | MOHAA script file; compiled by `code/parser` + `code/script` |
| TIKI | Model definition format (`.tik`): model, animations, sounds, events (`code/tiki`) |
| Skeletor | Skeletal animation engine (`code/skeletor`) |
| ubersound | Sound alias scripts (`ubersound/*.scr` in pk3) mapping alias names to files |
| `.urc` | UI resource (menu) files used by `code/uilib` |
| pk3 | Zip archive with game data; checksums used by `sv_pure` |
| Pure server | `sv_pure 1`: clients may only use paks the server has |
| svs time fixup | Saved times stored relative to the level and rebased on load (`SV_FinishSvsTimeFixups`) |
| TAG_GAME / TAG_CGAME | Zone memory tags owned by the game / cgame module |
| Map-script override | A `maps/<map>.scr` in the home path that replaces the stock map script (used by tests) |
