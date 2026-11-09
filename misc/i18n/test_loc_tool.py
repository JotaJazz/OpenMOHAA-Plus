"""Unit tests for loc_tool.py, with made-up texts (no game data)."""
import io
import os
import sys
import tempfile
import unittest
import zipfile
from contextlib import redirect_stdout

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import loc_tool  # noqa: E402


class LocTool(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        main = os.path.join(self.tmp.name, "main")
        os.makedirs(main)
        with zipfile.ZipFile(os.path.join(main, "Pak0.pk3"), "w") as z:
            z.writestr("global/localization.txt", '{ "Fire" "Fire" }\r\n{ "Old" "Old" }\r\n')
        with zipfile.ZipFile(os.path.join(main, "pak6.pk3"), "w") as z:   # later pak replaces the file
            z.writestr("global/localization.txt",
                       '{ "Fire" "Fire" }\r\n{ "&&& joined" "&&& joined" }\r\n{ "Caf\xe9" "Caf\xe9" }\r\n'
                       .encode("cp1252"))

    def run_tool(self, *args):
        out = io.StringIO()
        with redirect_stdout(out):
            code = loc_tool.main(list(args))
        return code, out.getvalue()

    def test_template_keeps_translations_and_writes_utf8(self):
        path = os.path.join(self.tmp.name, "lang", "es", "localization.txt")
        os.makedirs(os.path.dirname(path))
        with open(path, "w", encoding="utf-8") as f:
            f.write('{ "Fire" "Fuego" }\n{ "Gone" "Ya no" }\n')
        code, out = self.run_tool("template", "--game-dir", self.tmp.name, "--lang", "es", "--out", path)
        self.assertEqual(code, 0)
        self.assertIn("3 texts, 1 translated", out)
        entries = loc_tool.parse(open(path, encoding="utf-8").read())
        self.assertEqual(entries, [("Fire", "Fuego"), ("&&& joined", "&&& joined"), ("Café", "Café")])

    def test_check_reports_placeholders_charset_and_unknown_texts(self):
        path = os.path.join(self.tmp.name, "es.txt")
        with open(path, "w", encoding="utf-8") as f:
            f.write('{ "&&& joined" "se unió" }\n{ "Fire" "火" }\n{ "Nope" "No" }\n')
        code, out = self.run_tool("check", path, "--game-dir", self.tmp.name)
        self.assertEqual(code, 1)
        self.assertIn("3/3 translated", out)
        self.assertIn('placeholder: "&&& joined"', out)
        self.assertIn('charset: "火"', out)
        self.assertIn('unknown: "Nope"', out)

    def test_clean_translation_passes(self):
        path = os.path.join(self.tmp.name, "es.txt")
        with open(path, "w", encoding="utf-8") as f:
            f.write('{ "&&& joined" "&&& se unió" }\n')
        code, out = self.run_tool("check", path)
        self.assertEqual(code, 0)


if __name__ == "__main__":
    unittest.main()
