"""Dedicated server regression tests (need game data: OMOH_GAME_DIR).

Run: OMOH_GAME_DIR=<GAME_DIR> python3 -m unittest discover -s tests -v
"""

import os
import re
import socket
import time
import shutil
import subprocess
import tempfile
import unittest

from omoh import GameTestCase, require_game

BOTS = ["+set", "sv_maxbots", "2", "+set", "sv_minPlayers", "2"]


def connect_probe(port):
    """Send a raw connect request from 127.0.0.1 and return the server's reply text."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(3)
        sock.sendto(b"\xff\xff\xff\xff\x02connect \"\\x\\y\"", ("127.0.0.1", port))
        try:
            return sock.recvfrom(4096)[0][4:].decode(errors="replace")
        except socket.timeout:
            return ""


@require_game
class ServerSmoke(GameTestCase):
    def test_map_changes_with_bots(self):
        s = self.server("+set", "g_gametype", "1", *BOTS, "+map", "dm/mohdm6")
        s.wait_for(r"entered the battle", count=2)
        s.send("gamemap dm/mohdm1")
        s.wait_for(r"==== InitGame ====", count=2)
        s.send("map dm/mohdm2")
        s.wait_for(r"==== InitGame ====", count=3)
        s.send("restart")
        s.wait_for(r"entered the battle", count=6)
        self.assertEqual(s.quit(), 0)


@require_game
class ScriptCommands(GameTestCase):
    def test_registercmd_dispatches_to_label(self):
        # upstream #672
        self.home.write("maps/dm/mohdm1.scr", """main:
	registercmd "t672cmd" t672_label
	println "T672 registered"
end

t672_label local.a local.b:
	println ("T672 called a=" + local.a + " b=" + local.b)
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"T672 registered")
        s.send("t672cmd foo bar")
        s.wait_for(r"T672 called a=foo b=bar", timeout=20)
        s.send("map dm/mohdm2")
        s.wait_for(r"==== InitGame ====", count=2)
        s.send("t672cmd after_map_change")
        self.assertEqual(s.quit(), 0)
        self.assertNotIn("a=after_map_change", s.log())

    def test_item_amount_properties_are_readable(self):
        # upstream #920
        self.home.write("maps/dm/mohdm1.scr", """main:
	level waittill spawn
	local.h = spawn models/items/item_25_healthbox.tik
	waitframe
	println ("T920 amount=" + local.h.amount + " maxamount=" + local.h.maxamount + " dmamount=" + local.h.dmamount + " dmmaxamount=" + local.h.dmmaxamount)
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"T920 amount=25 maxamount=25 dmamount=25 dmmaxamount=25", timeout=60)
        self.assertEqual(s.quit(), 0)

    def test_waitforstate_waits_for_the_state(self):
        # upstream #702: a state that never happens must not wake the thread
        self.home.write("maps/dm/mohdm1.scr", """main:
	level waittill spawn
	thread watch
end

watch:
	while ($player.size < 1)
		wait 1
	$player[1] thread waitst
end

waitst:
	println "T702 waiting"
	self waitForState "NEVER_STATE"
	println "T702 woke"
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "1", *BOTS,
                        "+map", "dm/mohdm1")
        s.wait_for(r"T702 waiting", timeout=90)
        s.wait_for(r"entered the battle", count=2)
        time.sleep(10)
        self.assertEqual(s.quit(), 0)
        self.assertNotIn("T702 woke", s.log())


@require_game
class Bans(GameTestCase):
    def test_rehashbans_ignores_blank_lines(self):
        # upstream #926: removing a line's text leaves a blank line; the removed ban must not stay
        removed, kept = "127.0.0.1", "5.6.7.8"
        s = self.server("+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        s.send("banaddr " + removed)
        s.send("banaddr " + kept)
        s.wait_for(r"Added ban: " + re.escape(kept), timeout=20)
        self.assertIn("banned", connect_probe(s.port))   # the banned player tries to come back
        bans = self.home.file("serverbans.dat")
        with open(bans) as f:
            text = f.read()
        with open(bans, "w") as f:
            f.write(re.sub(r"^.*" + re.escape(removed) + r".*$", "", text, flags=re.M))
        mark = len(s.log())
        s.send("rehashbans")
        s.send("listbans")
        s.wait_for(r"Ban #1: ", timeout=20, start=mark)
        time.sleep(1)
        reply = connect_probe(s.port)
        self.assertEqual(s.quit(), 0)
        self.assertNotIn("banned", reply)
        listed = s.log()[mark:]
        self.assertNotIn(removed, listed)
        self.assertIn("Ban #1: " + kept, listed)


@require_game
class ModuleLifecycle(GameTestCase):
    @unittest.skipUnless(shutil.which("cc") and os.name == "posix", "needs a C compiler on POSIX")
    def test_map_change_with_resident_game_module(self):
        # upstream #918: emulate musl, where dlclose() never unloads the library
        shim_dir = tempfile.mkdtemp(prefix="omoh-shim-")
        self.addCleanup(shutil.rmtree, shim_dir, True)
        src = os.path.join(shim_dir, "nodlclose.c")
        lib = os.path.join(shim_dir, "nodlclose.so")
        with open(src, "w") as f:
            f.write("int dlclose(void *handle) { (void)handle; return 0; }\n")
        subprocess.run(["cc", "-shared", "-fPIC", "-o", lib, src], check=True)
        s = self.server("+set", "g_gametype", "1", *BOTS, "+map", "dm/mohdm6",
                        env={"LD_PRELOAD": lib})
        s.wait_for(r"entered the battle", count=2)
        s.send("gamemap dm/mohdm1")
        s.wait_for(r"==== InitGame ====", count=2)
        s.send("map dm/mohdm2")
        s.wait_for(r"==== InitGame ====", count=3)
        self.assertEqual(s.quit(), 0)


@require_game
class Diagnostics(GameTestCase):
    def test_freeze_watchdog_reports_and_recovers(self):
        s = self.server("+set", "developer", "1", "+set", "com_freezeWatch", "2",
                        "+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        s.send("freeze 5")
        s.wait_for(r"WATCHDOG: main loop frozen for 2\.\d seconds .*map dm/mohdm1", timeout=30)
        s.wait_for(r"main thread backtrace", timeout=10)
        s.wait_for(r"WATCHDOG: main loop recovered after [45]\.\d seconds", timeout=20)
        self.assertEqual(s.quit(), 0)
        with open(self.home.file("../freeze.log")) as f:
            self.assertIn("main loop frozen", f.read())

    def test_watchdog_is_off_by_default(self):
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        s.send("freeze 3")
        time.sleep(5)
        self.assertEqual(s.quit(), 0)
        self.assertNotIn("WATCHDOG", s.log())

    def test_crash_writes_a_report(self):
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        s.send("crash")
        s.proc.wait(30)
        self.assertNotEqual(s.proc.returncode, 0)
        with open(self.home.file("../crash.log")) as f:
            report = f.read()
        self.assertIn("==== crash: signal 11 ====", report)
        self.assertIn("map: dm/mohdm1", report)
        # frames are module(+offset) [address]; resolve offsets with addr2line -e <binary>
        self.assertGreaterEqual(len(re.findall(r"\(\+0x[0-9a-f]+\) \[0x[0-9a-f]+\]", report)), 3)
        self.assertIn("Crash report written to", s.log())


@require_game
@unittest.skipUnless(os.environ.get("OMOH_SLOW"), "slow (about 3 minutes); set OMOH_SLOW=1")
class SameMapRestart(GameTestCase):
    def test_script_waits_do_not_burst_after_restart(self):
        # upstream #287: a single-map rotation restarts the level; waits must not all fire at once
        # the thread starts before prespawn, as threads launched at the top of a map script do
        self.home.write("maps/dm/mohdm1.scr", """main:
	thread tick
	level waittill prespawn
	level waittill spawn
end

tick:
	while (1)
	{
		wait 2
		stuffsrv "echo T287TICK"
	}
end
""")
        s = self.server("+set", "g_gametype", "1", "+set", "timelimit", "1", *BOTS,
                        "+set", "sv_maplist", "dm/mohdm1", "+map", "dm/mohdm1", timeout=240)
        s.wait_for(r"^------ Server Initialization ------", count=2, timeout=200)  # the restart
        time.sleep(15)
        self.assertEqual(s.quit(), 0)
        worst = max((line.count("T287TICK") for line in s.log().splitlines()), default=0)
        self.assertLessEqual(worst, 2, "script waits fired in a burst after the restart")


if __name__ == "__main__":
    unittest.main()
