# Testing

Automated tests are minimal; most verification is a reproducible run of the real binaries.
Rule: **never open a visible window or play sound** during tests (the machine is shared with the
user). Reproduce the bug before the fix, then show it gone after the fix, with the same command.

## Automated

| What | Command |
|---|---|
| C++ unit test (`test_lz77`) | `cd .cmake && ctest --output-on-failure` (CI: `.github/workflows/unit-testing.yml`) |
| Docs checker tests | `python3 -I -m unittest discover -s misc/docs-check` |
| Docs checker | `python3 -I misc/docs-check/check_docs.py` (CI: `.github/workflows/docs-check.yml`) |

## Headless recipes

Prefer the dedicated server. For the client, always use all of:

```sh
SDL_VIDEODRIVER=offscreen ALSOFT_DRIVERS=null timeout -s KILL 150 ./openmohaa \
  +set ttycon 0 +set cl_playintro 0 +set s_volume 0 \
  +set fs_basepath <GAME_DIR> +set fs_homepath <HOME_DIR> +devmap training
```

| Need | How |
|---|---|
| Send console commands | Pipe stdin with `+set ttycon 0`: `( sleep 20; echo "map dm/mohdm2"; sleep 10; echo quit ) \| ./omohaaded ...` |
| Commands ignored by the client | Intro video is playing: add `+set cl_playintro 0` (or `developer 1`) |
| Client hangs on SIGTERM | Use `timeout -s KILL` |
| See the screen | `screenshotJPEG <name>` → `<HOME_DIR>/main/screenshots/<name>.jpg` |
| Run test script code | Map-script override: put `maps/<map>.scr` in `<HOME_DIR>/main/`; `println` needs `developer 1` |
| Players on a dedicated server | Bots: `+set sv_maxbots 2 +set sv_minPlayers 2` (they run the player state machine) |
| Save/load test | SP map (`training`; not `m1l1`, the player starts on a moving truck), `$player nodamage`, then `savegame t` / `loadgame t` |
| Measure audio | OpenAL Soft wave backend: `ALSOFT_CONF=<file>` with `[general]` `drivers=wave` and `[wave]` `file=<out.wav>`; compare RMS/peak of aligned windows |
| Measure frame rate | `+set com_speeds 1 +set r_swapInterval 0`, count `frame:` lines over wall-clock time |
| Emulate musl module behaviour | `LD_PRELOAD` a shared object defining `int dlclose(void *h) { return 0; }` |
| Memory errors | Scratch build with `-fsanitize=address` in C/CXX/linker flags; run with `ASAN_OPTIONS=detect_leaks=0` |
| Network probe (bans, pure checks) | Two processes on unique `net_port`s; server `developer 2` logs client state changes |

## Before merging a batch

Build the merged `main`, then: dedicated server through several map changes with 4 bots, and a
headless client through 2 map loads plus `vid_restart`; both must exit with code 0.
