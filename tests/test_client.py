"""Headless client regression tests (need game data: OMOH_GAME_DIR).

The client runs with SDL's offscreen video driver and OpenAL's null backend: no window, no sound.
Run: OMOH_GAME_DIR=<GAME_DIR> python3 -m unittest discover -s tests -v
"""

import glob
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


if __name__ == "__main__":
    unittest.main()
