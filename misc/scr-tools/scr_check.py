#!/usr/bin/env python3
# Copyright (C) 2026 the OpenMOHAA Plus contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Check MOHAA .scr scripts with the real script compiler, without starting the game window.

Starts a headless omohaaded with a temporary home, runs `compilescript` on every file and prints
errors as `file:line: error: message` (editor/CI friendly). Exit code 1 if any file fails.

Usage:
  python3 misc/scr-tools/scr_check.py --game-dir <GAME_DIR> [--bin-dir <BIN_DIR>] file.scr [...]

<GAME_DIR> contains main/ (the server needs a map to load the game module).
<BIN_DIR> defaults to the first of .cmake/RelWithDebInfo, .cmake/Release, .cmake/Debug, .cmake.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BIN_GUESSES = [".cmake/RelWithDebInfo", ".cmake/Release", ".cmake/Debug", ".cmake"]
LOCATION_RE = re.compile(r"^\((?P<file>[^,]+), (?P<line>\d+)\):")


def find_bin_dir(arg):
    candidates = [arg] if arg else [os.path.join(REPO, g) for g in BIN_GUESSES]
    for path in candidates:
        if path and os.path.isfile(os.path.join(path, "omohaaded")):
            return path
    sys.exit("omohaaded not found; pass --bin-dir")


def parse_errors(log, rel_paths):
    """Map each checked file to a list of (line, message) parsed from the server log."""
    errors = {rel: [] for rel in rel_paths}
    lines = log.splitlines()
    for i, line in enumerate(lines):
        m = LOCATION_RE.match(line.strip())
        if m and m.group("file") in errors:
            message = "syntax error"
            for prev in reversed(lines[max(0, i - 3):i]):
                if prev.strip() and prev.strip() != "parse error:":
                    message = prev.strip().rstrip(":")
                    break
            errors[m.group("file")].append((int(m.group("line")), message))
        fail = re.search(r"compilescript: (\S+) failed: (.*)", line)
        if fail and fail.group(1) in errors and not errors[fail.group(1)]:
            errors[fail.group(1)].append((1, fail.group(2).strip()))
    return errors


def check(files, game_dir, bin_dir, timeout=90):
    home = tempfile.mkdtemp(prefix="scr-check-")
    try:
        rel_paths = {}
        for index, path in enumerate(files):
            rel = f"scrcheck/{index}_{os.path.basename(path)}"
            os.makedirs(os.path.join(home, "main", "scrcheck"), exist_ok=True)
            shutil.copy(path, os.path.join(home, "main", rel))
            rel_paths[rel] = path

        commands = "".join(f"compilescript {rel} {rel}.bin\n" for rel in rel_paths) + "echo SCRCHECK_DONE\nquit\n"
        proc = subprocess.Popen(
            [os.path.join(bin_dir, "omohaaded"), "+set", "developer", "1", "+set", "fs_basepath", game_dir,
             "+set", "fs_homepath", home, "+set", "net_port", "0", "+set", "g_gametype", "1", "+map", "dm/mohdm1"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=bin_dir)
        # wait for the map, then send the commands
        output = []
        deadline = time.time() + timeout
        sent = False
        for raw in proc.stdout:
            text = raw.decode(errors="replace")
            output.append(text)
            if not sent and "Server Initialization Complete" in text:
                proc.stdin.write(commands.encode())
                proc.stdin.flush()
                sent = True
            if "SCRCHECK_DONE" in text or time.time() > deadline:
                break
        proc.kill()
        proc.wait()
        proc.stdin.close()
        proc.stdout.close()
        log = "".join(output)
        if not sent:
            sys.exit("the server did not start; log tail:\n" + log[-2000:])
        errors = parse_errors(log, rel_paths)
        missing = [rel for rel in rel_paths if not errors[rel] and not os.path.exists(os.path.join(home, "main", rel + ".bin"))]
        for rel in missing:
            errors[rel].append((1, "not compiled (see the server log)"))
        return {rel_paths[rel]: errs for rel, errs in errors.items()}
    finally:
        shutil.rmtree(home, ignore_errors=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--game-dir", required=True)
    parser.add_argument("--bin-dir")
    parser.add_argument("files", nargs="+")
    args = parser.parse_args(argv)
    results = check(args.files, args.game_dir, find_bin_dir(args.bin_dir))
    failed = 0
    for path, errors in results.items():
        for line, message in errors:
            print(f"{path}:{line}: error: {message}")
        failed += bool(errors)
    print(f"{len(results) - failed} ok, {failed} with errors")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
