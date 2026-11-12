#!/usr/bin/env python3
# Copyright (C) 2026 the OpenMOHAA Plus contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Generate the script command list for editors from the engine's own documentation dump.

Runs `dumpallclasses` on a headless omohaaded (or reads an existing g_allclasses.html) and writes
JSON: {"commands": [{"name", "signature", "doc", "classes"}]} used by misc/vscode-mohaa.

Usage:
  python3 misc/scr-tools/gen_editor_data.py --game-dir <GAME_DIR> [--bin-dir <BIN_DIR>] [-o OUT]
  python3 misc/scr-tools/gen_editor_data.py --html g_allclasses.html [-o OUT]
"""

import argparse
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scr_check import find_bin_dir, REPO  # noqa: E402

DEFAULT_OUT = os.path.join(REPO, "misc", "vscode-mohaa", "data", "commands.json")
CLASS_RE = re.compile(r'<h2>\s*<a name="(?P<cls>[^"]+)">', re.I)
CMD_RE = re.compile(r"<tt><b>(?P<name>[^<]+)</b>(?P<args>.*?)</tt>\s*<br>\s*<ul>(?P<doc>.*?)</ul>", re.I | re.S)


def strip_tags(text):
    return html.unescape(re.sub(r"<[^>]+>", "", text)).strip()


def parse_html(text):
    commands = {}
    positions = [(m.start(), m.group("cls")) for m in CLASS_RE.finditer(text)]
    for m in CMD_RE.finditer(text):
        cls = next((c for pos, c in reversed(positions) if pos < m.start()), "")
        name = strip_tags(m.group("name"))
        signature = name + " " + re.sub(r"\s+", " ", strip_tags(m.group("args"))).strip()
        key = (name, signature.strip())
        entry = commands.setdefault(key, {"name": name, "signature": signature.strip(),
                                          "doc": strip_tags(m.group("doc")), "classes": []})
        if cls and cls not in entry["classes"]:
            entry["classes"].append(cls)
    return sorted(commands.values(), key=lambda c: (c["name"], c["signature"]))


def dump_html(game_dir, bin_dir, timeout=90):
    home = tempfile.mkdtemp(prefix="scr-dump-")
    try:
        proc = subprocess.Popen(
            [os.path.join(bin_dir, "omohaaded"), "+set", "developer", "1", "+set", "fs_basepath", game_dir,
             "+set", "fs_homepath", home, "+set", "net_port", "0", "+set", "g_gametype", "1", "+map", "dm/mohdm1"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=bin_dir)
        deadline = time.time() + timeout
        for raw in proc.stdout:
            if b"Server Initialization Complete" in raw:
                proc.stdin.write(b"dumpallclasses\necho DUMP_DONE\n")
                proc.stdin.flush()
            if b"DUMP_DONE" in raw or time.time() > deadline:
                break
        proc.kill()
        proc.wait()
        proc.stdin.close()
        proc.stdout.close()
        path = os.path.join(home, "main", "g_allclasses.html")
        if not os.path.exists(path):
            sys.exit("dumpallclasses produced no file")
        with open(path, errors="replace") as f:
            return f.read()
    finally:
        shutil.rmtree(home, ignore_errors=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--game-dir")
    parser.add_argument("--bin-dir")
    parser.add_argument("--html")
    parser.add_argument("-o", "--out", default=DEFAULT_OUT)
    args = parser.parse_args(argv)
    if args.html:
        with open(args.html, errors="replace") as f:
            text = f.read()
    elif args.game_dir:
        text = dump_html(args.game_dir, find_bin_dir(args.bin_dir))
    else:
        parser.error("pass --game-dir or --html")
    commands = parse_html(text)
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w") as f:
        json.dump({"commands": commands}, f, indent=0, sort_keys=True)
        f.write("\n")
    print(f"{len(commands)} command signatures -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
