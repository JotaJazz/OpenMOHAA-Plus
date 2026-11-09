"""Tests for scr_check.py and gen_editor_data.py. Run: python3 -m unittest discover -s misc/scr-tools"""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_editor_data  # noqa: E402
import scr_check  # noqa: E402

DUMP = ('<h2> <a name="ScriptThread">ScriptThread (<i>ScriptThread</i>)</a></h2><BLOCKQUOTE>'
        '<p><tt><b>println</b>( <i>String string</i> )</tt><br><ul>Prints a string.</ul>'
        '<p><tt><b>waitframe</b></tt><br><ul>Wait for one server frame.</ul></BLOCKQUOTE>'
        '<h2> <a name="Entity">Entity</a></h2><BLOCKQUOTE>'
        '<p><tt><b>println</b>( <i>String string</i> )</tt><br><ul>Prints a string.</ul></BLOCKQUOTE>')

LOG = """parse error:
syntax error, unexpected TOKEN_LEFT_BRACES:
(scrcheck/1_bad.scr, 6):
\tif (1 {
^~^~^ Script file compile error:  Couldn't parse 'scrcheck/1_bad.scr'
compilescript: scrcheck/1_bad.scr failed: Script 'scrcheck/1_bad.scr' was not properly loaded
"""


class ParseTests(unittest.TestCase):
    def test_dump_is_parsed_and_merged_by_signature(self):
        commands = gen_editor_data.parse_html(DUMP)
        names = [(c["name"], c["signature"]) for c in commands]
        self.assertEqual(names, [("println", "println ( String string )"), ("waitframe", "waitframe")])
        self.assertEqual(commands[0]["classes"], ["ScriptThread", "Entity"])
        self.assertEqual(commands[0]["doc"], "Prints a string.")

    def test_compiler_errors_are_mapped_to_files_and_lines(self):
        errors = scr_check.parse_errors(LOG, ["scrcheck/0_good.scr", "scrcheck/1_bad.scr"])
        self.assertEqual(errors["scrcheck/0_good.scr"], [])
        self.assertEqual(errors["scrcheck/1_bad.scr"], [(6, "syntax error, unexpected TOKEN_LEFT_BRACES")])


@unittest.skipUnless(os.environ.get("OMOH_GAME_DIR"), "needs game data (OMOH_GAME_DIR)")
class EngineTests(unittest.TestCase):
    def test_check_real_scripts(self):
        with tempfile.TemporaryDirectory() as tmp:
            good = os.path.join(tmp, "good.scr")
            bad = os.path.join(tmp, "bad.scr")
            with open(good, "w") as f:
                f.write("main:\n\tprintln \"ok\"\nend\n")
            with open(bad, "w") as f:
                f.write("main:\n\tprintln \"x\"\nend\n\nother:\n\tif (1 {\n\t}\nend\n")
            results = scr_check.check([good, bad], os.environ["OMOH_GAME_DIR"],
                                      scr_check.find_bin_dir(os.environ.get("OMOH_BIN_DIR")))
            self.assertEqual(results[good], [])
            self.assertEqual([line for line, _ in results[bad]], [6])


if __name__ == "__main__":
    unittest.main()
