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
| `OMOH_KEEP_HOME` | keep each test's temporary home (logs, screenshots, saves) and print its path |

| File | Covers |
|---|---|
| `tests/omoh.py` | helpers: temporary home, start server/client headless, send console commands through stdin, wait for log lines |
| `tests/test_server.py` | libmoh HTTP/JSON (`create_api_request` against a local HTTP server), map changes with bots, `registercmd` (#672), item properties (#920), `waitForState` (#702), `rehashbans` (#926), resident game module (#918), same-map restart burst (#287, slow) |
| `tests/test_client.py` | map load + `vid_restart`, stopwatch after loading a save (#916, screenshot analysis), sound alias without extension (#589), `com_maxfps` (#530), pure server with an extra menu pak (#659) |

Every regression test was checked to fail on the code before its fix (upstream `a2f34019`) and to
pass after it. Issue numbers refer to `openmoh/openmohaa`. Add a test with every fix; recipes for new tests are in
[docs/dev/development/testing.md](../docs/dev/development/testing.md).
