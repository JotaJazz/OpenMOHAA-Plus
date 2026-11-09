# Game regression tests

Python (stdlib only) tests that run the real binaries headless: no window (SDL offscreen) and no
sound (OpenAL null backend). Each test reproduces a bug that was fixed and fails if it comes back.
They need the original game data, which is never committed.

```sh
cmake --build .cmake                                   # binaries in .cmake/RelWithDebInfo
OMOH_GAME_DIR=<GAME_DIR> python3 -I -m unittest discover -s tests -v
OMOH_GAME_DIR=<GAME_DIR> OMOH_SLOW=1 python3 -I -m unittest discover -s tests -v   # include slow tests
```

| Variable | Meaning |
|---|---|
| `OMOH_GAME_DIR` | game folder that contains `main/` (Allied Assault data); without it every test is skipped |
| `OMOH_BIN_DIR` | folder with `omohaaded`, `openmohaa`, `game.so`, `cgame.so`; default: the first of `.cmake/RelWithDebInfo`, `.cmake/Release`, `.cmake/Debug`, `.cmake`, `build` |
| `OMOH_SLOW` | also run slow tests (minutes) |
| `OMOH_MODS_DIR` | folder of mod `.pk3` files; each is loaded on a server with bots and must run without crashes or script errors (`OMOH_MOD_MAP`, `OMOH_MOD_GAMETYPE` pick map and gametype; defaults `dm/mohdm1`, 1) |
| `OMOH_KEEP_HOME` | keep each test's temporary home (logs, screenshots, saves) and print its path |

| File | Covers |
|---|---|
| `tests/omoh.py` | helpers: temporary home, start server/client headless, send console commands through stdin, wait for log lines |
| `tests/test_server.py` | freeze watchdog and crash report, map changes with bots, `registercmd` (#672), item properties (#920), `waitForState` (#702), `rehashbans` (#926), resident game module (#918), same-map restart burst (#287, slow), sv_perf frame timings, `sv_fps 40` frame rate and `wait`, gametype events (`player_damage`, `player_teamchange`, round events), libmoh HTTP/JSON (`create_api_request` against a local HTTP server), `sv_scripts`, reference mod `misc/mods/plus-stats` end to end |
| `tests/test_client.py` | map load + `vid_restart`, stopwatch after loading a save (#916, screenshot analysis), sound alias without extension (#589), `s_hrtf`, `com_maxfps` (#530), pure server with an extra menu pak (#659), disconnect reason after a kick, client on a 40 Hz server, pk3 downloads over UDP and HTTP (`sv_dlURL`) |

Every regression test was checked to fail on the code before its fix (upstream `a2f34019`) and to
pass after it. Issue numbers refer to `openmoh/openmohaa`. Add a test with every fix; recipes for new tests are in
[docs/dev/development/testing.md](../docs/dev/development/testing.md).
