# OpenMOHAA Plus: translations

## Playing in another language

Put the translation in your game or home folder under `main/lang/<code>/` (loose files or inside a
`.pk3`), then:

```
seta cl_language es
```

and restart the game. Texts the translation does not cover stay in the original language. Without
`cl_language` the game behaves as before.

## Making a translation

The game's texts come from your own copy of the game: this project ships no game text, so
translations are made and shared by the community.

1. Create the file from your game data (UTF-8; run it again later to add texts and keep your work):

   ```
   python3 misc/i18n/loc_tool.py template --game-dir <GAME_DIR> --lang es --out <home>/main/lang/es/localization.txt
   ```

   Each line is `{ "original text" "translation" }`. Translate the second text only.
2. Keep every `&&&`: it stands for a value the game inserts (a player name, a key).
3. Check it in the game: `cl_language es`, restart, then `localize "&&& has joined the Allies"`
   prints the translation. The console log says how many entries the file translated.
4. Check the file: `python3 misc/i18n/loc_tool.py check <file> --game-dir <GAME_DIR>` reports
   progress, broken `&&&`, unknown texts and characters the game cannot show.

## Characters

The game uses single-byte text (Latin-1 / Windows-1252) and its fonts only contain English letters.
Files saved as UTF-8 are converted when loaded. Accented letters and `ñ ¿ ¡` are drawn as their base
letter (`a`, `n`, `?`, `!`) unless fonts that contain them are installed:
[`zzz-plus-fonts.pk3`](09-plus-fonts.md) does that for the HUD, menu and console fonts. Languages outside Latin-1, such as Chinese (upstream #936), need
double-byte font support, which the game does not have.
