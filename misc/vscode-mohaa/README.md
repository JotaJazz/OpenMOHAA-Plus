# MOHAA Script for VS Code (OpenMOHAA Plus)

Syntax highlighting, completion and hover documentation for `.scr` scripts. The command list
(`data/commands.json`) comes from the engine itself, so it includes OpenMOHAA and OpenMOHAA Plus
commands.

Install from this folder: copy or symlink it into your VS Code extensions folder
(`~/.vscode/extensions/mohaa-scr`), or package it with `npx @vscode/vsce package` and install the
`.vsix`. Regenerate the command list after engine changes:

```sh
python3 misc/scr-tools/gen_editor_data.py --game-dir <GAME_DIR>
```

Check scripts for compile errors without starting the game:

```sh
python3 misc/scr-tools/scr_check.py --game-dir <GAME_DIR> path/to/script.scr
```
