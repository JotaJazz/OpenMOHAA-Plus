# Building

Full platform instructions (Windows, macOS, dependencies per OS):
[docs/markdown/04-coding/01-compiling.md](../../markdown/04-coding/01-compiling.md). This page is the
short Linux path used for development.

## Linux (Debian/Ubuntu, also WSL)

```sh
# dependencies: see the apt line in docs/markdown/04-coding/01-compiling.md
cmake -S . -B .cmake -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=<PREFIX>
cmake --build .cmake
cmake --install .cmake          # binaries and modules go to <PREFIX>/lib/openmohaa
```

- `.cmake/` is the conventional build dir; it is gitignored (it contains absolute local paths).
- Useful options (root `CMakeLists.txt`): `BUILD_SERVER`, `BUILD_CLIENT`, `BUILD_RENDERER_GL2`,
  `USE_RENDERER_DLOPEN`, `USE_OPENAL_DLOPEN`, `USE_INTERNAL_LIBS`.
- Scratch builds (agents, experiments) go to a separate directory: export with
  `git archive <ref> | tar -x -C <dir>` and configure there, so `.cmake/` stays the reference build.

## Game data

The engine needs the original game files (pk3) but never ships them. Run from the install dir:

```sh
cd <PREFIX>/lib/openmohaa
./openmohaa +set fs_basepath <GAME_DIR> +set fs_homepath <HOME_DIR>
./omohaaded +set fs_basepath <GAME_DIR> +set fs_homepath <HOME_DIR> +map dm/mohdm1
```

`<GAME_DIR>` contains `main/` (AA), and optionally `mainta/` (Spearhead) and `maintt/`
(Breakthrough). Use a dedicated `<HOME_DIR>` for tests so configs and saves of a real install are
not touched.

## Code style

`.clang-format` at the repo root; format changed code with `clang-format` (install it separately;
it is not part of the apt line above). Upstream rules in `CONTRIBUTING.md` (English, license
header: [docs/markdown/05-contributing/01-license-header.md](../../markdown/05-contributing/01-license-header.md)).
