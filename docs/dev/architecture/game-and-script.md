# Game module and script engine

Notes that are not obvious from a single file. Paths are relative to the repo root.

## Module lifecycle

| Step | Code | Notes |
|---|---|---|
| Load | `Sys_GetGameAPI` in `code/sys/new/sys_main_new.c` | dlopen `game.so`, call `GetGameAPI` |
| Init | `G_InitGame` in `code/fgame/g_main.cpp` | `L_InitEvents()` builds event tables, `Director.Reset()` |
| Same-map restart | `SV_MapRestart_f` → `SV_SpawnServer(..., differentmap=false)` | Module is **not** reloaded; globals survive |
| Shutdown | `SV_ShutdownGameProgs` in `code/server/sv_game.c` | `ge->Shutdown()`, unload, then free `TAG_GAME` only if really unloaded |

A module can stay resident after `dlclose()` (always on musl, sometimes on glibc). Code that runs
at init must not assume fresh globals. `L_ShutdownEvents` (`code/corepp/listener.cpp`) therefore
keeps the event tables: they are filled once from static constructors and cannot be rebuilt.

## Event system (`code/corepp`)

- Every script command or property is an `Event` with a type: `EV_NORMAL` (command), `EV_GETTER`
  (read `ent.field`), `EV_SETTER` (`ent.field = x`), `EV_RETURN` (command used on the right of `=`).
- Each type has its own lookup list; a property readable from scripts needs an explicit
  `EV_GETTER` registered under the same name. Pattern: `max_health` in `code/fgame/entity.cpp`.
- `ResponseDef` tables at the end of each class bind events to member functions.
- `Listener::Register(label, thread)` / `Unregister(label)` suspend and resume script threads;
  label `STRING_NULL` (0) resumes on the next frame (used by `waitmove`, `waitForState`).

## Script VM (`code/script`, `code/fgame/scriptmaster.cpp`)

- `Director` (`ScriptMaster`) owns compiled scripts, the string dictionary and the thread timer.
- `ScriptMaster::ExecuteRunning` runs every thread whose wait time is due against the Director's
  time. That time must follow `level.inttime`; `G_SetTime` syncs it (a stale time makes all
  waiting threads fire at once after a same-map restart).
- `ScriptMaster::Reset` frees scripts and strings on map change; anything holding a `GameScript*`
  or `const_str` across maps (e.g. `m_scriptCmds` from `registercmd`) must be cleared there.
- Commands registered with `registercmd` are dispatched by `G_ProcessScriptCommand`
  (`code/fgame/gamecmds.cpp`) after built-in commands.
- `println` prints only with `developer 1`.

## Time and savegames

- `Archiver::ArchiveSvsTime` (`code/fgame/archive.cpp`) stores times relative to `level.svsTime`
  and registers them; `SV_FinishSvsTimeFixups` (`code/server/sv_init.c`) adds `svs.time` back.
  Sound channel times use the same fixup list.
- Savegame layout has no general version gate on the client-side block; adding fields breaks
  old saves (see [boundaries.md](boundaries.md) rule 5).

## Entities and clients

- Entity numbers `0 .. game.maxclients-1` are client slots; map entities follow.
- Outside single player `game.maxclients = sv_maxclients + sv_maxbots` (`code/fgame/g_main.cpp`),
  so enabling bots shifts every map entity number. Scripts must find entities by `targetname`,
  not by number.
- Bots are real players (`code/fgame/playerbot.cpp`, `code/fgame/g_bot.cpp`) and run the player
  state machine; they are the easiest way to exercise player code on a dedicated server.

## Sounds referenced by the game

- `CacheResource` (`code/fgame/g_utils.cpp`) precaches only names containing `.wav`/`.mp3`;
  extensionless aliases load on first use.
- Aliases come from `ubersound` scripts parsed in `code/qcommon/alias.c`.
