"""Dedicated server regression tests (need game data: OMOH_GAME_DIR).

Run: OMOH_GAME_DIR=<GAME_DIR> python3 -m unittest discover -s tests -v
"""

import os
import re
import socket
import subprocess
import sys
import time
import shutil
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
import unittest

import omoh
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
class ServerTickRate(GameTestCase):
    SCRIPT = """main:
	level waittill spawn
	wait 2
	local.t = level.time
	local.n = 0
	while (level.time - local.t < 5)
	{
		waitframe
		local.n++
	}
	println ("TFPS frames " + local.n)
	local.t = level.time
	wait 1
	println ("TFPS wait1 " + (level.time - local.t))
end
"""

    def run_rate(self, fps):
        self.home.write("maps/dm/mohdm1.scr", self.SCRIPT)
        s = self.server("+set", "developer", "1", "+set", "sv_fps", str(fps), "+set", "g_gametype", "1",
                        "+map", "dm/mohdm1")
        log = s.wait_for(r"TFPS wait1", timeout=90)
        self.assertEqual(s.quit(), 0)
        frames = int(re.search(r"TFPS frames (\d+)", log).group(1))
        wait1 = float(re.search(r"TFPS wait1 ([\d.]+)", log).group(1))
        return frames, wait1, log

    def test_40hz_runs_40_frames_per_second(self):
        frames, wait1, log = self.run_rate(40)
        self.assertAlmostEqual(frames, 200, delta=2)
        self.assertAlmostEqual(wait1, 1.0, delta=0.03)
        self.assertNotIn("is not supported", log)

    def test_unsupported_rate_warns(self):
        self.home.write("maps/dm/mohdm1.scr", self.SCRIPT)
        s = self.server("+set", "sv_fps", "30", "+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"sv_fps 30 is not supported", timeout=60)
        self.assertEqual(s.quit(), 0)


@require_game
class GametypeEvents(GameTestCase):
    def test_player_damage_can_cancel_or_replace_damage(self):
        self.home.write("maps/dm/mohdm1.scr", """main:
	event_subscribe "player_damage" on_damage
	level waittill spawn
	thread run
end

run:
	while ($player.size < 1)
		wait 1
	wait 3
	local.p = $player[1]
	level.t_mode = 0
	local.h = local.p.health
	local.p damage $world 30 $world (0 0 0) (0 0 0) (0 0 0) 0 0 0 -1
	waitframe
	println ("TDMG cancel " + local.h + " " + local.p.health)
	level.t_mode = 1
	local.h = local.p.health
	local.p damage $world 30 $world (0 0 0) (0 0 0) (0 0 0) 0 0 0 -1
	waitframe
	println ("TDMG replace " + local.h + " " + local.p.health)
end

on_damage local.attacker local.damage:
	if (level.t_mode == 0)
		end 0
	end 5
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "1", *BOTS, "+map", "dm/mohdm1")
        log = s.wait_for(r"TDMG replace", timeout=120)
        self.assertEqual(s.quit(), 0)
        cancel = re.search(r"TDMG cancel ([\d.]+) ([\d.]+)", log)
        replace = re.search(r"TDMG replace ([\d.]+) ([\d.]+)", log)
        self.assertEqual(float(cancel.group(1)), float(cancel.group(2)))
        self.assertAlmostEqual(float(replace.group(1)) - float(replace.group(2)), 5, delta=0.5)

    def test_player_teamchange_can_refuse(self):
        self.home.write("maps/dm/mohdm1.scr", """main:
	event_subscribe "player_teamchange" on_team
	level waittill spawn
	thread run
end

run:
	while ($player.size < 1)
		wait 1
	wait 3
	local.p = $player[1]
	println ("TTEAM before " + local.p.dmteam)
	local.p join_team "axis"
	waitframe
	println ("TTEAM after_axis " + local.p.dmteam)
	local.p join_team "allies"
	waitframe
	println ("TTEAM after_allies " + local.p.dmteam)
end

on_team local.new local.old:
	println ("TTEAM event " + local.old + " -> " + local.new)
	if (local.new == "axis")
		end 0
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "2", *BOTS, "+map", "dm/mohdm1")
        log = s.wait_for(r"TTEAM after_allies", timeout=120)
        self.assertEqual(s.quit(), 0)
        self.assertRegex(log, r"TTEAM event \w+ -> axis")
        self.assertNotIn("TTEAM after_axis axis", log)
        self.assertIn("TTEAM after_allies allies", log)

    def test_round_events(self):
        self.home.write("maps/obj/obj_team2.scr", """main:
	event_subscribe "match_start" on_match
	event_subscribe "round_start" on_round_start
	event_subscribe "round_end" on_round_end
	level waittill spawn
	level waittill roundstart
	wait 3
	teamwin axis
end

on_match:
	println "TROUND match_start"
end

on_round_start local.round:
	println ("TROUND round_start " + local.round)
end

on_round_end local.winner local.axis local.allies:
	println ("TROUND round_end " + local.winner + " " + local.axis + " " + local.allies)
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "4", "+set", "sv_maxbots", "4",
                        "+set", "sv_minPlayers", "4", "+map", "obj/obj_team2")
        log = s.wait_for(r"TROUND round_end", timeout=180)
        self.assertEqual(s.quit(), 0)
        self.assertIn("TROUND match_start", log)
        self.assertIn("TROUND round_start 1", log)
        self.assertRegex(log, r"TROUND round_end axis 1 0")

    def test_match_pause_freezes_players_and_damage(self):
        self.home.write("maps/dm/mohdm1.scr", """main:
	level waittill spawn
	while ($player.size < 4) {
		wait 1
	}
	wait 5
	match_pause
	wait 0.5
	local.before = waitthread positions
	local.p = $player[1]
	local.health = local.p.health
	local.p damage $world 30 $world (0 0 0) (0 0 0) (0 0 0) 0 0 0 -1
	wait 3
	local.after = waitthread positions
	println ("TPAUSE paused=" + level.match_paused + " moved=" + (waitthread moved local.before local.after) + " damage=" + (local.health - local.p.health))
	match_resume
	wait 4
	local.later = waitthread positions
	println ("TPAUSE resumed=" + level.match_paused + " moved=" + (waitthread moved local.after local.later))
end

positions:
	for (local.i = 1; local.i <= $player.size; local.i++) {
		local.list[local.i] = $player[local.i].origin
	}
end local.list

moved local.a local.b:
	local.n = 0
	for (local.i = 1; local.i <= $player.size; local.i++) {
		if (vector_length (local.a[local.i] - local.b[local.i]) > 1) {
			local.n++
		}
	}
end local.n
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "2", "+set", "sv_maxbots", "4",
                        "+set", "sv_minPlayers", "4", "+map", "dm/mohdm1")
        log = s.wait_for(r"TPAUSE resumed", timeout=180)
        self.assertEqual(s.quit(), 0)
        self.assertRegex(log, r"TPAUSE paused=1 moved=0 damage=0")
        resumed = re.search(r"TPAUSE resumed=0 moved=(\d+)", log)
        self.assertGreater(int(resumed.group(1)), 0, log[-2000:])   # bots walk again
        self.assertIn("Match paused", log)
        self.assertIn("Match resumed", log)

    @unittest.skipUnless(os.environ.get("OMOH_SLOW"), "slow (about 2.5 minutes); set OMOH_SLOW=1")
    def test_match_pause_stops_the_time_limit(self):
        self.home.write("maps/dm/mohdm1.scr", """main:
	event_subscribe "level_intermission" on_intermission
	level waittill spawn
	wait 5
	match_pause
	wait 60
	match_resume
	println ("TCLOCK resumed at " + level.time)
end

on_intermission:
	println ("TCLOCK intermission at " + level.time)
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "2", "+set", "timelimit", "1",
                        "+set", "sv_maxbots", "2", "+set", "sv_minPlayers", "2", "+map", "dm/mohdm1")
        log = s.wait_for(r"TCLOCK intermission at", timeout=240)
        self.assertEqual(s.quit(), 0)
        self.assertIn("TCLOCK resumed at", log)                   # no intermission at 60 s, during the pause
        at = float(re.search(r"TCLOCK intermission at ([\d.]+)", log).group(1))
        self.assertGreater(at, 115)                                # 60 s of play plus 60 s paused

    def test_team_swap_in_team_deathmatch(self):
        self.home.write("maps/dm/mohdm1.scr", """main:
	level waittill spawn
	while ($player.size < 4) {
		wait 1
	}
	wait 3
	team_setscore "axis" 5
	local.before = waitthread teams
	team_swap
	wait 2
	local.after = waitthread teams
	local.alive = 0
	for (local.i = 1; local.i <= $player.size; local.i++) {
		if ($player[local.i].health > 0) {
			local.alive++
		}
	}
	local.ax = team_getscore "axis"
	local.al = team_getscore "allies"
	println ("TSWAP before=" + local.before + " after=" + local.after + " alive=" + local.alive + " score=" + local.ax + "/" + local.al)
end

teams:
	local.s = ""
	for (local.i = 1; local.i <= $player.size; local.i++) {
		local.s = local.s + $player[local.i].dmteam + ","
	}
end local.s
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "2", "+set", "sv_maxbots", "4",
                        "+set", "sv_minPlayers", "4", "+map", "dm/mohdm1")
        log = s.wait_for(r"TSWAP before", timeout=180)
        self.assertEqual(s.quit(), 0)
        m = re.search(r"TSWAP before=(\S*) after=(\S*) alive=(\d+) score=(\d+)/(\d+)", log)
        flip = {"axis": "allies", "allies": "axis"}
        self.assertEqual(m.group(2).split(",")[:-1], [flip[t] for t in m.group(1).split(",")[:-1]])
        self.assertEqual(m.group(3), "4")
        self.assertEqual(m.group(4, 5), ("0", "5"))

    def test_team_swap_at_halftime(self):
        self.home.write("maps/obj/obj_team2.scr", """main:
	event_subscribe "round_end" on_round_end
	event_subscribe "round_start" on_round_start
	level waittill spawn
	level waittill roundstart
	wait 3
	local.ax = team_getscore "axis"
	local.al = team_getscore "allies"
	if (local.ax + local.al == 0) {
		teamwin axis    // the map restarts every round: only end round 1
	}
end

teams:
	local.s = ""
	for (local.i = 1; local.i <= $player.size; local.i++) {
		local.s = local.s + $player[local.i].dmteam + ","
	}
end local.s

on_round_end local.winner local.axis local.allies:
	local.before = waitthread teams
	team_swap
	local.after = waitthread teams
	local.ax = team_getscore "axis"
	local.al = team_getscore "allies"
	println ("TSWAP before=" + local.before + " after=" + local.after + " score=" + local.ax + "/" + local.al)
end

on_round_start local.round:
	if (local.round == 2) {
		wait 3
		local.alive = 0
		for (local.i = 1; local.i <= $player.size; local.i++) {
			if ($player[local.i].health > 0) {
				local.alive++
			}
		}
		local.t = waitthread teams
		local.ax = team_getscore "axis"
		local.al = team_getscore "allies"
		println ("TSWAP round2 teams=" + local.t + " alive=" + local.alive + " score=" + local.ax + "/" + local.al)
	}
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "4", "+set", "sv_maxbots", "4",
                        "+set", "sv_minPlayers", "4", "+map", "obj/obj_team2")
        log = s.wait_for(r"TSWAP round2", timeout=240)
        self.assertEqual(s.quit(), 0)
        swap = re.search(r"TSWAP before=(\S*) after=(\S*) score=(\d+)/(\d+)", log)
        self.assertIsNotNone(swap, log[-3000:])
        before, after = swap.group(1).split(",")[:-1], swap.group(2).split(",")[:-1]
        flip = {"axis": "allies", "allies": "axis"}
        self.assertEqual(len(before), 4)
        self.assertEqual(after, [flip[t] for t in before])
        self.assertEqual(swap.group(3, 4), ("0", "1"))     # axis won round 1; the win moved with the side
        r2 = re.search(r"TSWAP round2 teams=(\S*) alive=(\d+) score=(\d+)/(\d+)", log)
        self.assertEqual(r2.group(1), swap.group(2))      # the new round keeps the swapped sides
        self.assertEqual(r2.group(2), "4")
        self.assertEqual(r2.group(3, 4), ("0", "1"))


@require_game
class ServerScripts(GameTestCase):
    def test_sv_scripts_run_on_every_level(self):
        self.home.write("plus/a.scr", 'main:\n\tprintln ("TSCR a " + (getcvar "mapname"))\nend\n')
        self.home.write("plus/b.scr", 'main:\n\tlevel waittill spawn\n\tprintln "TSCR b spawned"\nend\n')
        self.home.write("plus/broken.scr", "main:\n\tif (1 {\nend\n")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "1",
                        "+set", "sv_scripts", "plus/a.scr plus/missing.scr plus/broken.scr plus/b.scr",
                        "+map", "dm/mohdm1")
        s.wait_for(r"TSCR b spawned", timeout=90)
        s.send("map dm/mohdm2")
        log = s.wait_for(r"TSCR b spawned", count=2, timeout=90)
        self.assertEqual(s.quit(), 0)
        self.assertIn("TSCR a dm/mohdm1", log)
        self.assertIn("TSCR a dm/mohdm2", log)
        self.assertIn("sv_scripts: couldn't load 'plus/missing.scr'", log)
        self.assertRegex(log, r"sv_scripts: (couldn't load 'plus/broken.scr'|'plus/broken.scr' failed)")


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

    def test_rehashbans_reads_crlf_and_last_line_without_newline(self):
        # a ban file edited by hand: CRLF line ends, no newline after the last ban
        s = self.server("+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        with open(self.home.file("serverbans.dat"), "wb") as f:
            f.write(b"0 1.2.3.4 32:cheating\r\n0 5.6.7.8 32")
        mark, raw_mark = len(s.log()), os.path.getsize(s.log_path)
        s.send("rehashbans")
        s.send("listbans")
        s.wait_for(r"Ban #1: ", timeout=20, start=mark)
        time.sleep(1)
        self.assertEqual(s.quit(), 0)
        with open(s.log_path, "rb") as f:
            listed = f.read()[raw_mark:]
        self.assertIn(b"Ban #1: 1.2.3.4/32 - Reason: cheating\n", listed)
        self.assertIn(b"Ban #2: 5.6.7.8/32\n", listed)


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
class Profiling(GameTestCase):
    def test_sv_perf_reports_game_frames(self):
        s = self.server("+set", "g_gametype", "1", "+set", "sv_fps", "40", *BOTS, "+map", "dm/mohdm1")
        s.wait_for(r"entered the battle", count=2)
        s.send("sv_perf reset")
        time.sleep(5)
        mark = len(s.log())
        s.send("sv_perf")
        log = s.wait_for(r"^send ", start=mark, timeout=20)[mark:]
        self.assertEqual(s.quit(), 0)
        game = re.search(r"^game\s+n=(\d+) avg=([\d.]+)ms max=([\d.]+)ms", log, re.M)
        self.assertIsNotNone(game, log)
        self.assertGreaterEqual(int(game.group(1)), 150)       # ~40 frames per second for ~5 s
        self.assertLess(float(game.group(2)), 25.0)           # within the 40 Hz budget


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


@require_game
@unittest.skipUnless(os.environ.get("OMOH_SLOW"), "slow (about 1 minute); set OMOH_SLOW=1")
class NetworkFuzz(GameTestCase):
    def test_server_survives_malformed_packets(self):
        s = self.server("+set", "g_gametype", "1", "+set", "sv_maxclients", "8", "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        fuzzer = os.path.join(os.path.dirname(os.path.abspath(__file__)), "netfuzz.py")
        subprocess.run([sys.executable, "-I", fuzzer, str(s.port), "45", "1"], check=True,
                       stdout=subprocess.DEVNULL)
        self.assertIsNone(s.proc.poll(), "server died under malformed packets")
        s.send("status")
        s.wait_for(r"^map: dm/mohdm1", timeout=20)
        self.assertEqual(s.quit(), 0)


@require_game
class QueryFlood(GameTestCase):
    def test_query_floods_are_rate_limited_without_amplification(self):
        s = self.server("+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(0.002)
            for cmd in (b"getstatus", b"getinfo", b"getchallenge", b"rcon x status"):
                sent = replies = reply_bytes = 0
                packet = b"\xff\xff\xff\xff\x02" + cmd
                end = time.time() + 2
                while time.time() < end + 1:
                    if time.time() < end:
                        sock.sendto(packet, ("127.0.0.1", s.port))
                        sent += 1
                    try:
                        reply_bytes += len(sock.recv(65536))
                        replies += 1
                    except OSError:
                        pass
                self.assertGreater(replies, 0, cmd)          # still answers
                self.assertLessEqual(replies, 30, cmd)       # per-address limit
                self.assertLess(reply_bytes, sent * len(packet), cmd)  # no amplification
                time.sleep(1.5)
        self.assertEqual(s.quit(), 0)


def mod_paks():
    folder = os.environ.get("OMOH_MODS_DIR", "")
    if not os.path.isdir(folder):
        return []
    return sorted(os.path.join(folder, f) for f in os.listdir(folder) if f.lower().endswith(".pk3"))


@require_game
@unittest.skipUnless(mod_paks(), "set OMOH_MODS_DIR to a folder of mod .pk3 files")
class ModCompatibility(GameTestCase):
    """Each mod pk3 is loaded on a server with bots; it must run without crashes or script errors."""

    def test_mods_run_with_bots(self):
        for pak in mod_paks():
            with self.subTest(mod=os.path.basename(pak)):
                home = omoh.Home()
                self.addCleanup(home.cleanup)
                os.makedirs(os.path.join(home.path, "main"))
                shutil.copy(pak, os.path.join(home.path, "main"))
                s = omoh.server(home, "+set", "developer", "1", "+set", "g_gametype",
                                os.environ.get("OMOH_MOD_GAMETYPE", "1"), *BOTS,
                                "+map", os.environ.get("OMOH_MOD_MAP", "dm/mohdm1"))
                self.games.append(s)
                s.wait_for(r"entered the battle", count=2, timeout=120)
                time.sleep(20)
                self.assertIsNone(s.proc.poll(), "server died")
                self.assertEqual(s.quit(), 0)
                errors = [l for l in s.log().splitlines() if "Script Error" in l]
                self.assertFalse(errors, "\n".join(errors[:10]))


class _ApiHandler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def _reply(self, body):
        data = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        self._reply('{"id": 3, "title": "hello", "done": true, "list": [10, 20]}')

    def do_POST(self):
        self._reply(self.rfile.read(int(self.headers.get("Content-Length", 0))).decode())


@require_game
class LibMoh(GameTestCase):
    def setUp(self):
        super().setUp()
        self.http = HTTPServer(("127.0.0.1", 0), _ApiHandler)
        threading.Thread(target=self.http.serve_forever, daemon=True).start()
        self.addCleanup(self.http.shutdown)
        self.url = f"http://127.0.0.1:{self.http.server_address[1]}"

    def test_create_api_request_and_json(self):
        self.home.write("maps/dm/mohdm1.scr", """main:
	level waittill spawn
	local.r = create_api_request "URL/todo" "get" get_handler "ud1"
	println ("TAPI ret_get " + local.r)
	local.r = create_api_request "URL/echo" "post" post_handler "hello"
	println ("TAPI ret_post " + local.r)
	local.r = create_api_request "file:///etc/passwd" "get" file_handler
	println ("TAPI ret_file " + local.r)
	local.r = create_api_request "URL/todo" "delete" get_handler
	println ("TAPI ret_bad " + local.r)
	local.a[x] = 5
	println ("TAPI stringify " + (json_stringify local.a))
end

get_handler local.code local.resp local.ud:
	local.j = json_parse local.resp
	println ("TAPI get " + local.code + " " + local.j[title] + " " + local.j[done] + " " + local.j[list][1] + " " + local.ud)
end

post_handler local.code local.resp local.ud:
	local.j = json_parse local.resp
	println ("TAPI post " + local.code + " " + local.j[type] + " " + local.j[content])
end

file_handler local.code local.resp local.ud:
	println ("TAPI file " + local.code)
end
""".replace("URL", self.url))
        s = self.server("+set", "developer", "1", "+set", "sv_api_client", "1", "+set", "g_gametype", "1",
                        "+map", "dm/mohdm1")
        s.wait_for(r"TAPI get", timeout=60)
        s.wait_for(r"TAPI post", timeout=30)
        log = s.wait_for(r"TAPI file", timeout=30)
        self.assertEqual(s.quit(), 0)
        for line in ("TAPI ret_get 0", "TAPI ret_post 0", "TAPI ret_file 0", "TAPI ret_bad 1",
                     "TAPI get 200 hello 1 20 ud1", "TAPI post 200 string hello", "TAPI file -1"):
            self.assertIn(line, log)
        self.assertRegex(log, r'TAPI stringify \{"content":\{"x":\{"content":5,"type":"integer"\}\},"type":"array_object"\}')

    def test_api_client_is_off_by_default(self):
        self.home.write("maps/dm/mohdm1.scr", """main:
	level waittill spawn
	println ("TAPI ret " + (create_api_request "URL/todo" "get" h))
end

h local.code local.resp local.ud:
	println "TAPI called"
end
""".replace("URL", self.url))
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "1", "+map", "dm/mohdm1")
        log = s.wait_for(r"TAPI ret", timeout=60)
        time.sleep(2)
        self.assertEqual(s.quit(), 0)
        self.assertIn("TAPI ret 2", log)
        self.assertNotIn("TAPI called", s.log())


class _CaptureHandler(BaseHTTPRequestHandler):
    bodies = []

    def log_message(self, *args):
        pass

    def do_POST(self):
        _CaptureHandler.bodies.append(self.rfile.read(int(self.headers.get("Content-Length", 0))).decode())
        self.send_response(200)
        self.send_header("Content-Length", "2")
        self.end_headers()
        self.wfile.write(b"ok")


@require_game
class ReferenceMods(GameTestCase):
    def test_plus_match_halftime_draw_and_timeouts(self):
        mod = os.path.join(omoh.REPO, "misc", "mods", "plus-match", "global")
        shutil.copytree(mod, os.path.join(self.home.path, "main", "global"))
        # every round: axis wins after 4 s (the map restarts each round in objective mode)
        self.home.write("maps/obj/obj_team2.scr", """main:
	event_subscribe "level_intermission" on_intermission
	level waittill spawn
	level waittill roundstart
	wait 4
	teamwin axis
end

on_intermission:
	println "TMATCH intermission"
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "4", "+set", "sv_maxbots", "4",
                        "+set", "sv_minPlayers", "4", "+set", "sv_scripts", "global/plus/match.scr",
                        "+set", "plus_match_rounds", "4", "+set", "plus_match_timeouts", "1",
                        "+set", "plus_match_timeout_length", "2", "+map", "obj/obj_team2")
        s.wait_for(r"==== InitGame ====")
        s.wait_for(r"PLUS_MATCH loaded", timeout=120)
        s.wait_for(r"has entered the battle", count=4, timeout=120)
        time.sleep(1)
        s.send("timeout axis")
        s.wait_for(r"PLUS_MATCH timeout axis")
        s.wait_for(r"PLUS_MATCH resumed", timeout=20)         # timeout length 2 s
        mark = len(s.log())
        s.send("timeout axis")                                # none left this half
        s.send("plus_match_timeouts_axis")
        s.wait_for(r'"plus_match_timeouts_axis" is:"0', start=mark)
        s.wait_for(r"PLUS_MATCH draw", timeout=300)
        draw = len(s.log())
        log = s.wait_for(r"TMATCH intermission", start=draw, timeout=60)   # also fires between rounds
        s.send("fraglimit")
        s.wait_for(r'"fraglimit" is:"2')                      # lowered to the tied score to end the match
        self.assertEqual(s.quit(), 0)
        self.assertEqual(log[mark:].count("PLUS_MATCH timeout axis"), 0)
        rounds = re.findall(r"PLUS_MATCH round (\d+) winner (\w+) score (\d+)-(\d+)", log)
        self.assertEqual(rounds, [("1", "axis", "1", "0"), ("2", "axis", "2", "0"),
                                  ("3", "axis", "1", "2"), ("4", "axis", "2", "2")])
        self.assertLess(log.index("PLUS_MATCH round 2"), log.index("PLUS_MATCH halftime"))
        self.assertLess(log.index("PLUS_MATCH halftime"), log.index("PLUS_MATCH round 3"))
        self.assertIn("PLUS_MATCH draw", log)

    def test_plus_stats_posts_round_results(self):
        import json
        http = HTTPServer(("127.0.0.1", 0), _CaptureHandler)
        threading.Thread(target=http.serve_forever, daemon=True).start()
        self.addCleanup(http.shutdown)
        _CaptureHandler.bodies = []
        mod = os.path.join(omoh.REPO, "misc", "mods", "plus-stats", "global")
        shutil.copytree(mod, os.path.join(self.home.path, "main", "global"))
        self.home.write("maps/obj/obj_team2.scr", """main:
	level waittill spawn
	level waittill roundstart
	wait 5
	teamwin axis
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "4", "+set", "sv_maxbots", "4",
                        "+set", "sv_minPlayers", "4", "+set", "sv_scripts", "global/plus/stats.scr",
                        "+set", "sv_api_client", "1",
                        "+set", "plus_stats_url", f'"http://127.0.0.1:{http.server_address[1]}/round"',  # quoted: // starts a comment
                        "+map", "obj/obj_team2")
        s.wait_for(r"plus-stats: round posted, HTTP 200", timeout=180)
        self.assertEqual(s.quit(), 0)
        body = json.loads(_CaptureHandler.bodies[0])
        content = body["content"]
        self.assertEqual(content["map"]["content"], "obj/obj_team2")
        self.assertEqual(content["winner"]["content"], "axis")
        self.assertEqual(content["axis_wins"]["content"], 1)


@require_game
class InstallDetection(GameTestCase):
    def _empty_dir(self):
        path = tempfile.mkdtemp(prefix="omoh-empty-")
        self.addCleanup(shutil.rmtree, path, True)
        return path

    def test_finds_the_ea_install_folder_in_a_wine_prefix(self):
        prefix = self._empty_dir()
        ea = os.path.join(prefix, "drive_c", "Program Files (x86)", "EA GAMES")
        os.makedirs(ea)
        os.symlink(omoh.game_dir(), os.path.join(ea, "MOHAA"))
        s = omoh.server(self.home, "+set", "fs_basepath", self._empty_dir(), "+map", "dm/mohdm1",
                        env={"WINEPREFIX": prefix})
        self.games.append(s)
        s.wait_for(r"==== InitGame ====", timeout=120)
        s.send("fs_eapath")
        s.wait_for(r'"fs_eapath" is:".*EA GAMES/MOHAA"')
        self.assertEqual(s.quit(), 0)

    def test_missing_game_data_explains_what_to_do(self):
        s = omoh.server(self.home, "+set", "fs_basepath", self._empty_dir(), env={"WINEPREFIX": self._empty_dir()})
        self.games.append(s)
        log = s.wait_for(r"Start the game with \+set fs_basepath", timeout=60)
        self.assertIn("game data not found", log)
        self.assertIn("main/Pak0.pk3", log)


@require_game
class BotSettings(GameTestCase):
    def test_bot_weapons_and_skill(self):
        # upstream #684
        self.home.write("maps/dm/mohdm1.scr", """main:
	level waittill spawn
	while ($player.size < 4) {
		wait 1
	}
	wait 5
	for (local.i = 1; local.i <= $player.size; local.i++) {
		println ("TBOT " + $player[local.i].netname + " " + $player[local.i].getcurrentdmweapontype)
	}
end
""")
        s = self.server("+set", "developer", "1", "+set", "g_gametype", "2", "+set", "sv_maxbots", "4",
                        "+set", "sv_minPlayers", "4", "+set", "g_bot_weapons", "shotgun", "+set", "g_bot_skill", "5",
                        "+map", "dm/mohdm1")
        log = s.wait_for(r"TBOT .*", count=4, timeout=120)
        s.send("g_bot_skill 1")
        time.sleep(3)
        log = s.log()
        self.assertEqual(s.quit(), 0)
        weapons = re.findall(r"TBOT \S+ (\S+)", log)
        self.assertEqual(weapons, ["shotgun"] * 4)
        self.assertIn("Bot skill: Expert (reaction x0.3, aim spread x0.3, turn speed x1.8)", log)
        self.assertIn("Bot skill: Novice (reaction x2.0, aim spread x2.0, turn speed x0.6)", log)

    def _weapon_fire(self, seconds, script=None):
        if script:
            self.home.write("maps/dm/mohdm2.scr", script)
        s = self.server("+set", "g_gametype", "2", "+set", "sv_maxclients", "16", "+set", "sv_maxbots", "12",
                        "+set", "sv_minPlayers", "12", "+set", "g_showweaponfire", "1", "+map", "dm/mohdm2")
        s.wait_for(r"==== InitGame ====")
        time.sleep(seconds)
        log = s.log()
        self.assertEqual(s.quit(), 0)
        return re.findall(r"weapon fire: (.+) at \d+ ms", log)

    def test_bots_with_only_grenades_throw_them(self):
        # upstream #833: with grenades as their only weapon (gun game mods) bots took out an empty pistol
        shots = self._weapon_fire(40, """main:
	event_subscribe "player_spawned" on_spawn
	level waittill spawn
end

on_spawn:
	wait 0.5
	if (self == NULL || self.health <= 0)
		end
	self takeall
	self give models/weapons/m2frag_grenade.tik
	self ammo grenade 50
end
""")
        self.assertGreater(len(shots), 5, shots)
        self.assertEqual(set(shots), {"Frag Grenade"})

    @unittest.skipUnless(os.environ.get("OMOH_SLOW"), "slow (about 1.5 minutes); set OMOH_SLOW=1")
    def test_bots_throw_grenades_in_a_normal_match(self):
        shots = self._weapon_fire(80)
        self.assertTrue(any(name in ("Frag Grenade", "Stielhandgranate") for name in shots), shots)
        self.assertTrue(any(name not in ("Frag Grenade", "Stielhandgranate") for name in shots), shots)


if __name__ == "__main__":
    unittest.main()
