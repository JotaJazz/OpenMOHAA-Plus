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
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
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
