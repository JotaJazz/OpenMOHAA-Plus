"""Headless client regression tests (need game data: OMOH_GAME_DIR).

The client runs with SDL's offscreen video driver and OpenAL's null backend: no window, no sound.
Run: OMOH_GAME_DIR=<GAME_DIR> python3 -m unittest discover -s tests -v
"""

import glob
import tempfile
import re
import sys
import subprocess
import shutil
import functools
import http.server
import threading
import os
import struct
import time
import unittest
import zipfile

import omoh
from omoh import GameTestCase, require_game, game_dir


def read_tga(path):
    """Uncompressed 24/32-bit TGA -> (width, height, rows of (r, g, b) tuples, top row first)."""
    with open(path, "rb") as f:
        data = f.read()
    id_len, cmap_type, img_type = data[0], data[1], data[2]
    if img_type != 2 or cmap_type != 0:
        raise ValueError(f"unsupported TGA type {img_type}")
    width, height = struct.unpack("<HH", data[12:16])
    bpp, desc = data[16], data[17]
    step = bpp // 8
    pixels = data[18 + id_len:]
    rows = []
    for y in range(height):
        row = pixels[y * width * step:(y + 1) * width * step]
        rows.append([(row[x + 2], row[x + 1], row[x]) for x in range(0, width * step, step)])
    if not desc & 0x20:  # origin at the bottom
        rows.reverse()
    return width, height, rows


def bright_pixels(path, x0, x1, y0, y1, threshold=200):
    """Count near-white pixels in a region given as fractions of the screen size."""
    width, height, rows = read_tga(path)
    count = 0
    for row in rows[int(y0 * height):int(y1 * height)]:
        for r, g, b in row[int(x0 * width):int(x1 * width)]:
            if r > threshold and g > threshold and b > threshold:
                count += 1
    return count


@require_game
class ClientSmoke(GameTestCase):
    def test_map_loads_and_vid_restart(self):
        c = self.client("+devmap", "training")
        c.wait_for(r"^Server: training")
        time.sleep(10)
        c.send("vid_restart")
        c.wait_for(r"finished CL_Vid_Restart_f", timeout=60)
        c.send("devmap dm/mohdm1")
        c.wait_for(r"^Server: dm/mohdm1", timeout=120)
        self.assertEqual(c.quit(), 0)


@require_game
class SaveLoad(GameTestCase):
    def test_stopwatch_survives_loading_a_save(self):
        # upstream #916: the HUD stopwatch (left edge, mid height) must be drawn after a load
        self.home.write("maps/training.scr", """main:
	level waittill spawn
	$player nodamage
	println "T916 spawned"
	wait 8
	$player stopwatch 120
	println "T916 started"
end
""")
        region = (0.0, 0.12, 0.38, 0.62)
        c = self.client("+set", "developer", "1", "+devmap", "training")
        c.wait_for(r"T916 spawned", timeout=120)
        time.sleep(3)
        c.send("screenshot t916_none")
        c.wait_for(r"T916 started", timeout=30)
        time.sleep(3)
        c.send("screenshot t916_before")
        time.sleep(2)
        c.send("savegame t916")
        c.wait_for(r"Saving to t916", timeout=30)
        time.sleep(3)
        mark = len(c.log())
        c.send("loadgame t916")
        c.wait_for(r"Loading game\.\.\.t916", timeout=60, start=mark)
        time.sleep(15)
        c.send("screenshot t916_after")
        time.sleep(3)
        self.assertEqual(c.quit(), 0)
        shots = {n: self.home.file(f"screenshots/t916_{n}.tga") for n in ("none", "before", "after")}
        counts = {n: bright_pixels(p, *region) for n, p in shots.items()}
        self.assertGreater(counts["before"], counts["none"] + 300, counts)
        self.assertGreater(counts["after"], counts["none"] + 300, f"stopwatch missing after load: {counts}")


@require_game
class Sounds(GameTestCase):
    def test_alias_without_wav_extension_loads(self):
        # upstream #589: impact_playerleavewater1 points to a file name without ".wav"
        self.home.write("maps/training.scr", """main:
	level waittill spawn
	$player nodamage
	wait 2
	println "T589 playing"
	$player playsound impact_playerleavewater1
end
""")
        c = self.client("+set", "developer", "1", "+devmap", "training")
        c.wait_for(r"T589 playing", timeout=120)
        c.wait_for(r"S_LoadSound: sound/characters/body_mvmtwater_01", timeout=20)
        time.sleep(3)
        self.assertEqual(c.quit(), 0)
        self.assertNotIn("Couldn't load sound: sound/characters/body_mvmtwater_01", c.log())


@require_game
class Hrtf(GameTestCase):
    def test_s_hrtf_forces_hrtf(self):
        c = self.client("+set", "s_hrtf", "1")
        c.wait_for(r"OpenAL: HRTF (enabled|disabled)", timeout=60)
        self.assertEqual(c.quit(), 0)
        self.assertRegex(c.log(), r"OpenAL: HRTF enabled \(")

    def test_hrtf_off_by_default(self):
        c = self.client()
        c.wait_for(r"OpenAL: HRTF (enabled|disabled)", timeout=60)
        self.assertEqual(c.quit(), 0)
        self.assertIn("OpenAL: HRTF disabled", c.log())


@require_game
class FrameRate(GameTestCase):
    def test_maxfps_caps_at_the_requested_rate(self):
        # upstream #530: 1000 / com_maxfps used to truncate (85 -> ~90 fps)
        c = self.client("+set", "com_maxfps", "85", "+set", "com_maxfpsUnfocused", "0",
                        "+set", "com_maxfpsMinimized", "0", "+set", "r_swapInterval", "0",
                        "+set", "com_speeds", "1")
        c.wait_for(r"^frame:", count=200, timeout=120)
        time.sleep(3)
        n0, t0 = c.log().count("\nframe:"), time.time()
        time.sleep(10)
        n1, t1 = c.log().count("\nframe:"), time.time()
        c.quit()
        fps = (n1 - n0) / (t1 - t0)
        if fps < 70:
            self.skipTest(f"machine too slow to reach 85 fps ({fps:.1f})")
        self.assertAlmostEqual(fps, 85, delta=3)


@require_game
class Disconnect(GameTestCase):
    def test_client_shows_the_disconnect_reason(self):
        s = self.server("+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        client_home = omoh.Home()
        self.addCleanup(client_home.cleanup)
        c = omoh.client(client_home, "+connect", f"127.0.0.1:{s.port}")
        self.games.append(c)
        s.wait_for(r"entered the battle", timeout=120)
        s.send("kicknum 0")
        # kicks arrive as a "droperror" packet; mid-connection drops (unpure client) as a reliable
        # "disconnect" command: both must leave the reason in com_errorMessage
        c.wait_for(r"Server disconnected: \S|Server dropped connection\. Reason: \S", timeout=30)
        c.send("com_errorMessage")
        c.wait_for(r'"com_errorMessage" is:"[^"]+"', timeout=10)
        c.quit()
        self.assertEqual(s.quit(), 0)


@require_game
class FastServer(GameTestCase):
    def test_client_plays_on_a_40hz_server(self):
        s = self.server("+set", "sv_fps", "40", "+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        client_home = omoh.Home()
        self.addCleanup(client_home.cleanup)
        c = omoh.client(client_home, "+set", "snaps", "40", "+connect", f"127.0.0.1:{s.port}")
        self.games.append(c)
        s.wait_for(r"entered the battle", timeout=120)
        time.sleep(15)
        mark = len(s.log())
        s.send("status")
        status = s.wait_for(r"^map: dm/mohdm1", timeout=20, start=mark)[mark:]
        c.quit()
        self.assertEqual(s.quit(), 0)
        self.assertRegex(status, r"\n\s*0\s+\d+\s+\d+\s")      # client 0 still connected
        for bad in ("ERROR:", "Server disconnected", "Server dropped connection"):
            self.assertNotIn(bad, c.log())

    def _fire_intervals(self, sv_fps, weapon):
        home = omoh.Home()
        self.addCleanup(home.cleanup)
        c = omoh.client(home, "+set", "developer", "1", "+set", "sv_fps", str(sv_fps), "+set", "g_showweaponfire", "1",
                        "+set", "g_gametype", "2", "+set", "sv_maxclients", "2", "+devmap", "dm/mohdm1")
        self.games.append(c)
        c.wait_for(r"==== InitGame ====", timeout=120)
        time.sleep(10)
        c.send(f"primarydmweapon {weapon}")
        c.send("join_team allies")
        time.sleep(6)
        c.send("popmenu 0")
        c.send("popmenu 0")
        time.sleep(2)
        c.send("+attack")
        time.sleep(2)
        c.send("-attack")
        time.sleep(1)
        self.assertEqual(c.quit(), 0)
        times = [int(t) for t in re.findall(r"weapon fire: .* at (\d+) ms", c.log())]
        self.assertGreater(len(times), 5)
        return sorted({b - a for a, b in zip(times, times[1:])})

    def test_40hz_keeps_the_20hz_fire_rate(self):
        # the fire delay is checked once per frame: a 20 Hz server rounds it up to 50 ms steps
        self.assertEqual(self._fire_intervals(40, "mg"), [150])    # BAR: 400 rpm, not 480
        self.assertEqual(self._fire_intervals(40, "smg"), [100])   # Thompson: 600 rpm


@require_game
class PureServer(GameTestCase):
    MENU_FILES = ("ui/connecting.urc", "ui/errormessage.urc", "textures/mohmenu/internet.tga")

    def _extra_pak(self, path):
        """A client-only pk3 with byte-identical copies of files the main menu reads."""
        wanted = {name.lower(): None for name in self.MENU_FILES}
        for pk3 in sorted(glob.glob(os.path.join(game_dir(), "main", "*.pk3")), key=str.lower):
            with zipfile.ZipFile(pk3) as z:
                for info in z.infolist():
                    if info.filename.lower() in wanted:
                        wanted[info.filename.lower()] = (info.filename, z.read(info))
        found = [v for v in wanted.values() if v]
        if not found:
            self.skipTest("menu files not found in the game pk3s")
        with zipfile.ZipFile(path, "w") as out:
            for name, data in found:
                out.writestr(name, data)

    def test_client_with_extra_menu_pak_joins_pure_server(self):
        # upstream #659: paks read in the menu before connecting were reported to the pure server
        s = self.server("+set", "developer", "1", "+set", "sv_pure", "1", "+set", "g_gametype", "1",
                        "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        client_home = omoh.Home()
        self.addCleanup(client_home.cleanup)
        os.makedirs(os.path.join(client_home.path, "main"))
        self._extra_pak(os.path.join(client_home.path, "main", "zzz_t659.pk3"))
        c = omoh.client(client_home, "+connect", f"127.0.0.1:{s.port}")
        self.games.append(c)
        log = s.wait_for(r"entered the battle|Unpure", timeout=120)
        c.quit()
        self.assertEqual(s.quit(), 0)
        self.assertNotIn("Unpure", log)


def _quiet_handler(directory):
    class Handler(http.server.SimpleHTTPRequestHandler):
        requests = []

        def log_message(self, fmt, *args):
            Handler.requests.append(fmt % args)
    return functools.partial(Handler, directory=directory), Handler


@require_game
class Downloads(GameTestCase):
    def _mod_server(self, dl_url=None):
        import zipfile as zf
        server_home = omoh.Home()
        self.addCleanup(server_home.cleanup)
        os.makedirs(os.path.join(server_home.path, "main"))
        with zf.ZipFile(os.path.join(server_home.path, "main", "zzz_dltest.pk3"), "w") as z:
            z.writestr("maps/dm/mohdm1.scr", "main:\n\tlevel waittill spawn\n\tprintln \"DLTEST\"\nend\n")
        args = ["+set", "sv_allowDownload", "1", "+set", "g_gametype", "1"]
        if dl_url:
            args += ["+set", "sv_dlURL", f'"{dl_url}"']   # quoted: // starts a comment
        s = omoh.server(server_home, *args, "+map", "dm/mohdm1")
        self.games.append(s)
        s.wait_for(r"==== InitGame ====")
        return server_home, s

    def _join(self, s):
        client_home = omoh.Home()
        self.addCleanup(client_home.cleanup)
        c = omoh.client(client_home, "+set", "cl_allowDownload", "1", "+connect", f"127.0.0.1:{s.port}")
        self.games.append(c)
        s.wait_for(r"entered the battle", timeout=120)
        c.quit()
        return os.path.join(client_home.path, "main", "zzz_dltest.pk3")

    def test_udp_download(self):
        _, s = self._mod_server()
        pak = self._join(s)
        self.assertEqual(s.quit(), 0)
        self.assertTrue(os.path.exists(pak))
        self.assertIn("clientDownload", s.log())

    def test_http_redirect_download(self):
        handler, record = _quiet_handler(".")
        web = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        threading.Thread(target=web.serve_forever, daemon=True).start()
        self.addCleanup(web.server_close)
        self.addCleanup(web.shutdown)
        home, s = self._mod_server(f"http://127.0.0.1:{web.server_address[1]}")
        web.RequestHandlerClass.keywords["directory"] = home.path   # serve the server's home folder
        pak = self._join(s)
        self.assertEqual(s.quit(), 0)
        self.assertTrue(os.path.exists(pak))
        self.assertTrue(any("zzz_dltest.pk3" in r and " 200 " in r for r in record.requests), record.requests)
        self.assertNotIn("clientDownload", s.log())


def green_pixels(path, x0, x1, y0, y1):
    """Count saturated green pixels (the HUD health bar) in a region given as screen fractions."""
    width, height, rows = read_tga(path)
    return sum(1 for row in rows[int(y0 * height):int(y1 * height)]
               for r, g, b in row[int(x0 * width):int(x1 * width)] if g > 120 and r < 80 and b < 80)


@require_game
class HudScale(GameTestCase):
    def shot(self, scale):
        home = omoh.Home()
        self.addCleanup(home.cleanup)
        # sv_maxclients 2: a one-player local game waits on the loading screen's continue button
        c = omoh.client(home, "+set", "developer", "1", "+set", "r_mode", "-1", "+set", "r_customwidth", "1920",
                        "+set", "r_customheight", "1080", "+set", "ui_hudscale", str(scale), "+set", "g_gametype",
                        "2", "+set", "sv_maxclients", "2", "+devmap", "dm/mohdm1")
        self.games.append(c)
        c.wait_for(r"==== InitGame ====", timeout=120)
        time.sleep(10)
        c.send("join_team allies")
        time.sleep(1)
        c.send("primarydmweapon rifle")
        time.sleep(8)
        c.send("screenshot hud")
        time.sleep(3)
        self.assertEqual(c.quit(), 0)
        return green_pixels(os.path.join(home.path, "main", "screenshots", "hud.tga"), 0.0, 0.1, 0.75, 1.0)

    def test_crosshair_scale_and_size(self):
        # upstream #911: crosshair size independent of the texture (cg_crosshair_size) and a multiplier
        c = self.client("+set", "developer", "1", "+set", "r_mode", "-1",
                        "+set", "r_customwidth", "1280", "+set", "r_customheight", "720", "+set", "g_gametype", "2",
                        "+set", "sv_maxclients", "2", "+devmap", "dm/mohdm1")
        c.wait_for(r"==== InitGame ====", timeout=120)
        time.sleep(10)
        c.send("primarydmweapon smg")
        c.send("join_team allies")
        time.sleep(6)
        c.send("popmenu 0")    # team and weapon menus cover the crosshair
        c.send("popmenu 0")
        time.sleep(2)
        c.send("freezeplayer")  # a still camera (random spawn point, the player may still fall), no message
        time.sleep(3)
        names = ("off", "normal", "scale3", "size64")
        for cvars in ("ui_crosshair 0", "ui_crosshair 1", "cg_crosshair_scale 3",
                      "cg_crosshair_scale 1; cg_crosshair_size 64"):
            for cvar in cvars.split("; "):
                c.send(cvar)
            time.sleep(1)
            c.send("screenshot silent")    # a named screenshot centerprints "Wrote ..." over the crosshair
            time.sleep(1)
        self.assertEqual(c.quit(), 0)
        shots = dict(zip(names, sorted(glob.glob(self.home.file("screenshots/shot*.tga")))))
        self.assertEqual(len(shots), 4)
        def changed(name):    # pixels the crosshair changes in the screen center
            w, h, rows = read_tga(shots[name])
            _, _, off = read_tga(shots["off"])
            y0, y1, x0, x1 = int(h * 0.39), int(h * 0.61), int(w * 0.4375), int(w * 0.5625)
            return sum(1 for y in range(y0, y1) for x in range(x0, x1)
                       if sum(abs(a - b) for a, b in zip(rows[y][x], off[y][x])) > 60)

        count = {n: changed(n) for n in ("normal", "scale3", "size64")}
        self.assertGreater(count["normal"], 5, count)
        self.assertGreater(count["scale3"], count["normal"] * 2, count)
        self.assertGreater(count["size64"], count["normal"] * 2, count)

    def test_ui_hudscale_scales_the_hud(self):
        # upstream #585: at 1920x1080 the HUD is drawn at its 640x480 pixel size; ui_hudscale enlarges it
        normal, double = self.shot(1), self.shot(2)
        self.assertGreater(normal, 300)
        self.assertGreater(double / normal, 3.0, (normal, double))   # twice the size on both axes


@require_game
@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("gcc"), "needs Linux and gcc (LD_PRELOAD)")
class ServerBrowser(GameTestCase):
    def test_slow_dns_does_not_freeze_the_game(self):
        # upstream #376: refreshing the internet server list froze the game while master DNS lookups hung
        shim = os.path.join(self.home.path, "slowdns.so")
        subprocess.run(["gcc", "-shared", "-fPIC", "-o", shim,
                        os.path.join(os.path.dirname(os.path.abspath(__file__)), "slowdns.c"), "-ldl"], check=True)
        c = self.client("+set", "developer", "1", env={"LD_PRELOAD": shim})
        c.wait_for(r"Opening IP socket", timeout=60)
        time.sleep(10)
        c.send("pushmenu joinnetgame")
        time.sleep(2)
        c.send("widgetcommand netgamelist refreshserverlist")
        time.sleep(0.5)
        mark = len(c.log())
        start = time.time()
        c.send("echo BROWSER_ALIVE")
        c.wait_for(r"^BROWSER_ALIVE", start=mark, timeout=30)
        self.assertLess(time.time() - start, 2.0)          # was 5 s per master lookup
        time.sleep(6)                                       # past the master socket timeout
        mark = len(c.log())
        c.send("echo STDIN_ALIVE")                          # a never-created master socket closed fd 0
        c.wait_for(r"^STDIN_ALIVE", start=mark, timeout=10)
        self.assertIn("SLOWDNS getaddrinfo(master.", c.log())
        self.assertEqual(c.quit(), 0)


    NO_MASTERS = [arg for i in range(5) for arg in ("+set", f"com_master{i}_host", "")]

    def _list(self, c, expect):
        mark = len(c.log())
        c.send("widgetcommand netgamelist refreshserverlist")
        time.sleep(6)
        c.send("widgetcommand netgamelist printlist")
        return c.wait_for(expect, start=mark, timeout=20)[mark:]

    def test_favorites_history_and_search(self):
        alpha = self.server("+set", "sv_hostname", "Alpha", "+map", "dm/mohdm1")
        home_b = omoh.Home()
        self.addCleanup(home_b.cleanup)
        bravo = omoh.server(home_b, "+set", "sv_hostname", "Bravo", "+map", "dm/mohdm1")
        self.games.append(bravo)
        for s in (alpha, bravo):
            s.wait_for(r"==== InitGame ====")
            s.send("net_gamespy_port")
        qa, qb = (int(re.findall(r'"net_gamespy_port" is:"(\d+)', s.wait_for(r'"net_gamespy_port" is:"\d+'))[-1])
                  for s in (alpha, bravo))
        client_home = omoh.Home()
        self.addCleanup(client_home.cleanup)
        client_home.write("serverfavorites.txt", f"127.0.0.1:{alpha.port} {qa}\n")
        c = omoh.client(client_home, "+set", "developer", "1", *self.NO_MASTERS, "+connect", f"127.0.0.1:{bravo.port}")
        self.games.append(c)
        bravo.wait_for(r"entered the battle", timeout=120)
        c.send(f"fav_add 127.0.0.1:{bravo.port} {qb}")
        c.wait_for(r"Added favorite 127.0.0.1")
        c.send(f"fav_del 127.0.0.1:{bravo.port}")
        c.wait_for(r"Removed favorite 127.0.0.1")
        c.send("disconnect")
        time.sleep(3)
        c.send("history_list")
        c.wait_for(rf"127\.0\.0\.1:{bravo.port}")
        c.send("pushmenu joinnetgame")
        time.sleep(2)

        c.send("dm_browser_show 2")
        listed = self._list(c, r"Server list \(\d+\):(\n  .*)*\n.*Bravo")
        self.assertRegex(listed, r"\* .*Alpha \| 127\.0\.0\.1:%d" % alpha.port)
        self.assertRegex(listed, r"Bravo \(recent\) \| 127\.0\.0\.1:%d" % bravo.port)

        c.send("dm_browser_show 0")
        c.send("dm_browser_search Alp")
        listed = self._list(c, r"Server list \((\d+)\):")
        self.assertIn("Alpha", listed)
        self.assertNotIn("Bravo", listed)
        self.assertEqual(c.quit(), 0)

        history = open(client_home.file("serverhistory.txt")).read()
        self.assertIn(f"127.0.0.1:{bravo.port} {qb}", history)     # query port published by the server


@require_game
class Translations(GameTestCase):
    def test_cl_language_overrides_the_game_texts(self):
        # saved as UTF-8, like a modern editor does; the game shows Latin-1
        self.home.write("lang/es/localization.txt",
                        '{ "Health" "Salud" }\r\n{ "&&& has joined the Allies" "&&& se unió a los Aliados" }\r\n')
        for language, health in (("es", "Salud"), ("", "Health")):
            c = self.client("+set", "developer", "1", "+set", "cl_language", language)
            c.wait_for(r"Opening IP socket", timeout=60)
            c.send("localize Health")
            c.send("localize &&& has joined the Allies")
            c.wait_for(r'"&&& has joined the Allies" -> ')
            self.assertEqual(c.quit(), 0)
            with open(c.log_path, "rb") as f:
                log = f.read()
            self.assertIn(f'"Health" -> "{health}"'.encode(), log)
            if language:
                self.assertIn("2 entries translated by lang/es/localization.txt".encode(), log)
                self.assertIn('-> "&&& se unió a los Aliados"'.encode("latin-1"), log)


@require_game
class HitSounds(GameTestCase):
    SCRIPT = """main:
	level waittill spawn
	while (1) {
		local.human = NIL
		local.enemy = NIL
		for (local.i = 1; local.i <= $player.size; local.i++) {
			local.p = $player[local.i]
			if (!(isBot local.p) && local.p.dmteam != "spectator") {
				local.human = local.p
			}
		}
		if (local.human) {
			for (local.i = 1; local.i <= $player.size; local.i++) {
				local.p = $player[local.i]
				if ((isBot local.p) && local.p.dmteam != local.human.dmteam && local.p.health > 0) {
					local.enemy = local.p
				}
			}
		}
		if (local.enemy) {
			local.enemy damage local.human 5 local.human (0 0 0) (0 0 0) (0 0 0) 0 0 0 -1
			println "THIT hit"
		}
		wait 1
	}
end
"""

    def _play(self, enabled):
        server_home = omoh.Home()
        self.addCleanup(server_home.cleanup)
        server_home.write("maps/dm/mohdm1.scr", self.SCRIPT)
        s = omoh.server(server_home, "+set", "developer", "1", "+set", "g_gametype", "2", "+set", "sv_maxbots", "2",
                        "+set", "sv_minPlayers", "2", "+set", "g_hitsounds", str(enabled),
                        "+set", "g_hitsound", "sound/plus_hit_test.wav", "+map", "dm/mohdm1")
        self.games.append(s)
        s.wait_for(r"==== InitGame ====")
        client_home = omoh.Home()
        self.addCleanup(client_home.cleanup)
        c = omoh.client(client_home, "+set", "developer", "1", "+connect", f"127.0.0.1:{s.port}")
        self.games.append(c)
        s.wait_for(r"UnnamedSoldier.* has entered the battle", timeout=120)
        time.sleep(3)
        c.send("primarydmweapon smg")
        c.send("join_team allies")
        s.wait_for(r"THIT hit", count=3, timeout=60)
        c.quit()
        self.assertEqual(s.quit(), 0)
        return c.log()

    def test_server_enabled_hit_sound_reaches_the_attacker(self):
        self.assertIn("sound/plus_hit_test.wav", self._play(1))

    def test_hit_sounds_are_off_by_default(self):
        self.assertNotIn("sound/plus_hit_test.wav", self._play(0))


def _build_fonts(pk3):
    """Run misc/fonts/make_fonts.py (needs Pillow and DejaVu, outside this isolated interpreter)."""
    tool = os.path.join(omoh.REPO, "misc", "fonts", "make_fonts.py")
    python = shutil.which("python3")
    if not python:
        return False
    result = subprocess.run([python, tool, "--out", pk3], cwd=tempfile.gettempdir(), capture_output=True, text=True)
    return result.returncode == 0


@require_game
class Fonts(GameTestCase):
    def test_generated_fonts_load_and_draw(self):
        pk3 = os.path.join(self.home.path, "main", "zzz-plus-fonts.pk3")
        os.makedirs(os.path.dirname(pk3), exist_ok=True)
        if not _build_fonts(pk3):
            self.skipTest("misc/fonts/make_fonts.py needs Pillow and the DejaVu fonts")
        self.home.write("lang/es/localization.txt", '{ "Health" "Salud ñáé ¿¡" }\r\n')
        c = self.client("+set", "developer", "1", "+set", "cl_language", "es", "+set", "ui_hudscale", "2",
                        "+set", "g_gametype", "2", "+set", "sv_maxclients", "2", "+devmap", "dm/mohdm1")
        c.wait_for(r"==== InitGame ====", timeout=120)
        time.sleep(10)
        c.send("primarydmweapon smg")
        c.send("join_team allies")
        time.sleep(5)
        c.send("screenshot silent")
        time.sleep(2)
        self.assertEqual(c.quit(), 0)
        log = c.log()
        self.assertIn("zzz-plus-fonts.pk3", log)
        for bad in ("Error parsing font", "Couldn't load font", "top-down", "character in font"):
            self.assertNotIn(bad, log)
        self.assertTrue(glob.glob(self.home.file("screenshots/shot*.tga")))


@require_game
class Admin(GameTestCase):
    CONFIG = """// test policy
role mod allow admin.status admin.kick* admin.ban* cvar.*
role mod deny admin.ban
account jota roles mod
account jota allow admin.say
account jota deny cvar.fraglimit
"""

    def test_admin_accounts_roles_and_sessions(self):
        self.home.write("admin_config.txt", self.CONFIG)
        s = self.server("+set", "g_gametype", "1", "+map", "dm/mohdm1")
        s.wait_for(r"==== InitGame ====")
        s.send("admin_add jota pass-1234")
        s.send("admin_add nobody pass-5678")
        s.wait_for(r"Account nobody created")
        client_home = omoh.Home()
        self.addCleanup(client_home.cleanup)
        c = omoh.client(client_home, "+connect", f"127.0.0.1:{s.port}")
        self.games.append(c)
        s.wait_for(r"entered the battle", timeout=120)

        def run(line, expect):
            mark = len(c.log())
            c.send(line)
            return c.wait_for(expect, start=mark, timeout=20)[mark:]

        run("ad_status", r"Not logged in")
        run("ad_login jota wrong", r"Login failed")
        run("ad_login jota pass-1234", r"Too many failed logins")       # throttled right after a failure
        time.sleep(2.5)
        run("ad_login jota pass-1234", r"Logged in as jota")
        run("ad_help", r"Admin commands for jota: ad_logout ad_status ad_kick ad_say ad_set")   # admin.kick* does not cover admin.clientkick
        run("ad_status", r"map: dm/mohdm1")                               # console output reaches the admin
        run("ad_ban 0", r"Permission denied: admin.ban")                  # deny wins over admin.ban*
        run("ad_rcon status", r"Permission denied: admin.rcon")
        run("ad_set timelimit 15", r"timelimit is '15'")
        run("ad_set fraglimit 1", r"Permission denied: cvar.fraglimit")
        run("ad_set rconPassword x", r"Permission denied: cvar.rconPassword")  # cvar.* does not grant rcon
        run('ad_kick "x;quit"', r"Usage: ad_kick")                         # no second command
        s.send("admin_passwd jota pass-9999")
        c.wait_for(r"Admin session ended: password changed", timeout=20)
        run("ad_status", r"Not logged in")
        c.quit()
        s.send("timelimit")
        s.wait_for(r'"timelimit" is:"15"')
        self.assertEqual(s.quit(), 0)

        audit = open(self.home.file("admin.log")).read()
        for entry in (r"account=jota failed: ad_login", r"account=jota throttled: ad_login",
                      r"account=jota ok: ad_login", r"account=jota denied: ad_ban 0",
                      r"account=jota ok: status", r"account=jota ok: ad_set timelimit 15",
                      r"console \"\" account=jota ok: admin_passwd"):
            self.assertRegex(audit, entry)
        accounts = open(self.home.file("admin_accounts.txt")).read()
        for secret in ("pass-1234", "pass-5678", "pass-9999"):
            self.assertNotIn(secret, audit + accounts)
        self.assertRegex(accounts, r"account nobody 1 pbkdf2-sha256\$100000\$[0-9a-f]{32}\$[0-9a-f]{64}")


if __name__ == "__main__":
    unittest.main()
