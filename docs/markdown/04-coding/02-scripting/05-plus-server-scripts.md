# OpenMOHAA Plus server scripts (sv_scripts)

Server mods usually hook into every map by overriding stock files such as `global/DMprecache.scr`,
which breaks when two mods do it. OpenMOHAA Plus runs a list of server scripts on every level
instead:

```
set sv_scripts "global/mymod/main.scr global/stats.scr"
```

- Scripts run in order, right after the map script, on every map load and map restart.
- Each script starts at its `main` label; use `level waittill spawn` or the
  [script events](03-plus-events.md) (`event_subscribe`) to act later.
- A missing script or one that doesn't compile prints a `sv_scripts:` message and is skipped; the
  server keeps running.
- Ship the scripts in a pk3 or in the server's home folder.
