#!/usr/bin/env python3
# Copyright (C) 2026 the OpenMOHAA Plus contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Translation helper for OpenMOHAA Plus (cl_language).

The game's texts are read from your own copy of the game; this repository ships no game text.

  template: create or update lang/<code>/localization.txt from the game's global/localization.txt,
            keeping the translations already in the file:
    python3 misc/i18n/loc_tool.py template --game-dir <GAME_DIR> --lang es [--out FILE]
  check: report progress and problems of a translation:
    python3 misc/i18n/loc_tool.py check FILE [--game-dir <GAME_DIR>]

Files are written in UTF-8; the game converts them to its single-byte charset (Latin-1). Characters
the game's fonts do not have are drawn as their base letter (á as a) unless the translation ships
fonts that have them.
"""
import argparse
import glob
import os
import re
import sys
import zipfile

ENTRY = re.compile(r'\{\s*"([^"]*)"\s*"([^"]*)"\s*\}')


def parse(text):
    """Return [(key, value)] from a localization file's text."""
    return ENTRY.findall(text)


def decode(data):
    """Language files may be UTF-8 or Latin-1 (the game's own files are Latin-1)."""
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return data.decode("cp1252", errors="replace")


def game_entries(game_dir, game="main"):
    """Entries of global/localization*.txt in the game's pk3 files; later paks win, first entry wins."""
    files = {}
    for pak in sorted(glob.glob(os.path.join(game_dir, game, "*.pk3")), key=str.lower):
        with zipfile.ZipFile(pak) as z:
            for name in z.namelist():
                base = name.lower()
                if base.startswith("global/localization") and base.endswith(".txt"):
                    files[base] = z.read(name)
    entries, seen = [], set()
    for name in sorted(files):
        for key, value in parse(decode(files[name])):
            if key not in seen:
                seen.add(key)
                entries.append((key, value))
    return entries


def render(entries):
    return "".join('{ "%s" "%s" }\n' % kv for kv in entries)


def template(game, existing):
    """Game entries with the existing translations kept; untranslated texts keep the English value."""
    done = dict(existing)
    return [(key, done.get(key, value)) for key, value in game]


def problems(entries, game=None):
    found = []
    keys = {k for k, _ in game} if game is not None else None
    for key, value in entries:
        if key.count("&&&") != value.count("&&&"):
            found.append(f'placeholder: "{key}" has {key.count("&&&")} &&&, the translation {value.count("&&&")}')
        try:
            value.encode("cp1252")
        except UnicodeEncodeError:
            found.append(f'charset: "{value}" has characters the game cannot show (only Latin-1)')
        if keys is not None and key not in keys:
            found.append(f'unknown: "{key}" is not a text of this game')
    return found


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    t = sub.add_parser("template")
    t.add_argument("--game-dir", required=True)
    t.add_argument("--game", default="main", help="main, mainta (Spearhead) or maintt (Breakthrough)")
    t.add_argument("--lang", required=True)
    t.add_argument("--out")
    c = sub.add_parser("check")
    c.add_argument("file")
    c.add_argument("--game-dir")
    c.add_argument("--game", default="main")
    args = ap.parse_args(argv)

    if args.cmd == "template":
        if not re.fullmatch(r"[A-Za-z0-9_-]+", args.lang):
            ap.error("--lang: letters, digits, _ or -")
        game = game_entries(args.game_dir, args.game)
        if not game:
            ap.error(f"no global/localization*.txt in {os.path.join(args.game_dir, args.game)}/*.pk3")
        out = args.out or os.path.join("lang", args.lang, "localization.txt")
        existing = parse(decode(open(out, "rb").read())) if os.path.exists(out) else []
        entries = template(game, existing)
        os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
        with open(out, "w", encoding="utf-8", newline="\r\n") as f:
            f.write(render(entries))
        translated = sum(1 for (k, v), (_, e) in zip(entries, game) if v != e)
        print(f"{out}: {len(entries)} texts, {translated} translated")
        return 0

    entries = parse(decode(open(args.file, "rb").read()))
    game = game_entries(args.game_dir, args.game) if args.game_dir else None
    english = dict(game) if game else {}
    translated = sum(1 for k, v in entries if v != english.get(k, k))
    total = len(game) if game else len(entries)
    print(f"{args.file}: {translated}/{total} translated ({100 * translated // max(total, 1)}%)")
    found = problems(entries, game)
    for p in found:
        print("  " + p)
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
