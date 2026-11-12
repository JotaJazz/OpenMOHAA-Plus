# Copyright (C) 2026 the OpenMOHAA Plus contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Helpers to run OpenMOHAA Plus binaries headless in tests.

Never opens a window or plays sound: the client always runs with the SDL offscreen video driver
and OpenAL's null (or wave-file) backend. Console commands are sent through piped stdin, which
the engine reads as console input when it is not a TTY.

Environment:
  OMOH_GAME_DIR  game folder containing main/ (required; tests skip without it)
  OMOH_BIN_DIR   folder with openmohaa, omohaaded, game.so, cgame.so (default: build dir guess)
"""

import os
import re
import shutil
import socket
import subprocess
import tempfile
import time
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_BIN_GUESSES = [".cmake/RelWithDebInfo", ".cmake/Release", ".cmake/Debug", ".cmake", "build"]


def game_dir():
    path = os.environ.get("OMOH_GAME_DIR", "")
    return path if path and os.path.isdir(os.path.join(path, "main")) else None


def bin_dir():
    env = os.environ.get("OMOH_BIN_DIR")
    candidates = [env] if env else [os.path.join(REPO, g) for g in _BIN_GUESSES]
    for path in candidates:
        if path and os.path.isfile(os.path.join(path, "omohaaded")):
            return path
    return None


def require_game(test):
    """Class decorator: skip unless game data and binaries are available."""
    if not game_dir():
        return unittest.skip("set OMOH_GAME_DIR to a game folder with main/")(test)
    if not bin_dir():
        return unittest.skip("set OMOH_BIN_DIR to the folder with omohaaded/openmohaa")(test)
    return test


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Home:
    """A temporary fs_homepath; write files under main/ (e.g. map-script overrides)."""

    def __init__(self):
        self.path = tempfile.mkdtemp(prefix="omoh-test-")

    def write(self, rel, content, mode="w"):
        full = os.path.join(self.path, "main", rel)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, mode) as f:
            f.write(content)
        return full

    def file(self, rel):
        return os.path.join(self.path, "main", rel)

    def cleanup(self):
        shutil.rmtree(self.path, ignore_errors=True)


class Game:
    """A running omohaaded or headless openmohaa, with stdin as its console."""

    def __init__(self, home, args, client=False, env=None, timeout=180):
        self.home = home
        self.log_path = os.path.join(home.path, "client.log" if client else "server.log")
        self.port = free_port()
        exe = os.path.join(bin_dir(), "openmohaa" if client else "omohaaded")
        cmd = [exe, "+set", "fs_basepath", game_dir(), "+set", "fs_homepath", home.path,
               "+set", "net_port", str(self.port)]
        full_env = dict(os.environ)
        if client:
            cmd += ["+set", "cl_playintro", "0", "+set", "s_volume", "0"]
            full_env.update({"SDL_VIDEODRIVER": "offscreen", "ALSOFT_DRIVERS": "null"})
        full_env.update(env or {})
        self._log = open(self.log_path, "w")
        self.proc = subprocess.Popen(cmd + list(args), stdin=subprocess.PIPE, stdout=self._log,
                                     stderr=subprocess.STDOUT, cwd=bin_dir(), env=full_env)
        self.deadline = time.time() + timeout

    def log(self):
        with open(self.log_path, errors="replace") as f:
            return f.read()

    def send(self, line):
        self.proc.stdin.write((line + "\n").encode())
        self.proc.stdin.flush()

    def wait_for(self, pattern, timeout=90, count=1, start=0):
        """Wait until `pattern` appears `count` times after offset `start`; return the log."""
        regex = re.compile(pattern, re.M)
        end = min(time.time() + timeout, self.deadline)
        while time.time() < end:
            text = self.log()
            if len(regex.findall(text[start:])) >= count:
                return text
            if self.proc.poll() is not None:
                break
            time.sleep(0.5)
        raise AssertionError(f"timed out waiting for {pattern!r} (x{count}); log tail:\n"
                             + self.log()[-3000:])

    def quit(self, timeout=20):
        """Send quit and return the exit code (SIGKILL after timeout: the client can hang)."""
        if self.proc.poll() is None:
            try:
                self.send("quit")
            except (BrokenPipeError, OSError):
                pass
            try:
                self.proc.wait(timeout)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        self._close()
        return self.proc.returncode

    def kill(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()
        self._close()

    def _close(self):
        for f in (self.proc.stdin, self._log):
            if f and not f.closed:
                try:
                    f.close()
                except OSError:
                    pass


def server(home, *args, **kw):
    return Game(home, ["+set", "dedicated", "1"] + list(args), **kw)


def client(home, *args, **kw):
    return Game(home, list(args), client=True, **kw)


class GameTestCase(unittest.TestCase):
    """Gives each test a temporary home and kills leftover processes."""

    def setUp(self):
        self.home = Home()
        self.games = []

    def tearDown(self):
        for g in self.games:
            g.kill()
        if os.environ.get("OMOH_KEEP_HOME"):
            print(f"\nkept home: {self.home.path}")
        else:
            self.home.cleanup()

    def server(self, *args, **kw):
        g = server(self.home, *args, **kw)
        self.games.append(g)
        return g

    def client(self, *args, **kw):
        g = client(self.home, *args, **kw)
        self.games.append(g)
        return g
