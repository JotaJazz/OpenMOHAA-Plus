# Copyright (C) 2026 the OpenMOHAA Plus contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Unit tests for check_docs.py. Run: python3 -m unittest discover -s misc/docs-check"""

import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_docs  # noqa: E402


class RepoTestCase(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = self._tmp.name
        self.write("code/server/sv_init.c", "")
        self.write("docs/dev/README.md", "# Index\n")

    def tearDown(self):
        self._tmp.cleanup()

    def write(self, rel, content):
        path = os.path.join(self.root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(textwrap.dedent(content))

    def problems(self):
        return check_docs.check_repo(self.root)

    def kinds(self):
        return sorted({p.kind for p in self.problems()})


class LinkTests(RepoTestCase):
    def test_valid_relative_link_passes(self):
        self.write("AGENTS.md", "See [index](docs/dev/README.md#top).\n")
        self.assertEqual(self.problems(), [])

    def test_broken_relative_link_is_reported(self):
        self.write("AGENTS.md", "See [missing](docs/dev/nope.md).\n")
        self.assertEqual(self.kinds(), ["broken-link"])

    def test_external_and_anchor_links_are_ignored(self):
        self.write("AGENTS.md", "[a](https://example.com) [b](#section) [c](mailto:noreply@example.com)\n")
        self.assertEqual(self.problems(), [])


class PathTests(RepoTestCase):
    def test_existing_backtick_path_passes(self):
        self.write("AGENTS.md", "Edit `code/server/sv_init.c:53` carefully.\n")
        self.assertEqual(self.problems(), [])

    def test_missing_backtick_path_is_reported(self):
        self.write("AGENTS.md", "Edit `code/server/sv_gone.c`.\n")
        self.assertEqual(self.kinds(), ["missing-path"])

    def test_placeholders_globs_and_non_paths_are_ignored(self):
        self.write("AGENTS.md", "`code/<module>/x.c` `code/*/CMakeLists.txt` `+set developer 1` `a/b`\n")
        self.assertEqual(self.problems(), [])

    def test_paths_inside_code_fences_are_ignored(self):
        self.write("AGENTS.md", "```\n`code/not/there.c`\n```\n~~~\n`code/not/there.c`\n~~~\n")
        self.assertEqual(self.problems(), [])

    def test_gitignored_path_is_reported_in_a_git_repo(self):
        if not shutil.which("git"):
            self.skipTest("git not available")
        subprocess.run(["git", "init", "-q", self.root], check=True)
        self.write(".gitignore", "/code/generated\n")
        self.write("code/generated/out.cpp", "")
        self.write("AGENTS.md", "Built into `code/generated` and `code/server/sv_init.c`.\n")
        problems = self.problems()
        self.assertEqual([p.kind for p in problems], ["missing-path"])
        self.assertIn("code/generated", problems[0].message)


class PersonalDataTests(RepoTestCase):
    def test_home_and_mount_paths_are_reported(self):
        self.write("AGENTS.md", "Data in /mnt/x/Game and /home/alice/build.\n")
        self.assertEqual(self.kinds(), ["personal-data"])
        self.assertEqual(len(self.problems()), 2)

    def test_windows_drive_path_is_reported(self):
        self.write("AGENTS.md", "Install to X:\\Games\\MOHAA.\n")
        self.assertEqual(self.kinds(), ["personal-data"])

    def test_placeholders_are_allowed(self):
        self.write("AGENTS.md", "Use /home/<user>/x, /mnt/<drive>/y and <GAME_DIR>.\n")
        self.assertEqual(self.problems(), [])

    def test_private_ip_is_reported_but_documentation_ranges_pass(self):
        self.write("AGENTS.md", "bad 10.20.30.40; ok 192.0.2.1 198.51.100.7 203.0.113.9 127.0.0.1 0.0.0.0\n")
        problems = self.problems()
        self.assertEqual([p.kind for p in problems], ["personal-data"])
        self.assertIn("10.20.30.40", problems[0].message)

    def test_version_numbers_are_not_ips(self):
        self.write("AGENTS.md", "OpenAL Soft 1.24.3 and curl 8.15.0 and v0.83.0.1-beta\n")
        self.assertEqual(self.problems(), [])

    def test_secret_tokens_are_reported(self):
        self.write("AGENTS.md", "token ghp_" + "a" * 36 + "\n")
        self.assertEqual(self.kinds(), ["personal-data"])

    def test_ip_at_sentence_end_is_reported(self):
        self.write("AGENTS.md", "Server at 10.0.0.5.\n")
        self.assertEqual(self.kinds(), ["personal-data"])

    def test_more_personal_path_forms_are_reported(self):
        for text in ("/Users/alice/x", "/media/alice", "c:\\games", "C:/Users/alice", "\\\\wsl$\\Distro",
                     "github_pat_" + "a" * 30, "gho_" + "b" * 36, "alice@example.org"):
            with self.subTest(text=text):
                self.write("AGENTS.md", f"see {text} here\n")
                self.assertEqual(self.kinds(), ["personal-data"])

    def test_noreply_and_placeholder_emails_pass(self):
        self.write("AGENTS.md", "noreply@anthropic.com and user@users.noreply.github.com <EMAIL>\n")
        self.assertEqual(self.problems(), [])


class MemoryTests(RepoTestCase):
    MEMORY = "docs/ai/PROJECT_MEMORY.md"

    def test_stage_with_three_bullets_passes(self):
        self.write(self.MEMORY, "# Memory\n\n## 2026-01-02 — topic\n- a\n- b\n- c\n")
        self.assertEqual(self.problems(), [])

    def test_stage_with_four_bullets_is_reported(self):
        self.write(self.MEMORY, "# Memory\n\n## 2026-01-02 — topic\n- a\n- b\n- c\n- d\n")
        self.assertEqual(self.kinds(), ["memory-stage"])

    def test_malformed_stage_heading_is_reported(self):
        self.write(self.MEMORY, "# Memory\n\n## Jan 2 topic\n- a\n")
        self.assertEqual(self.kinds(), ["memory-stage"])


class ScopeTests(RepoTestCase):
    def test_upstream_docs_and_local_plans_are_not_checked(self):
        self.write("docs/markdown/page.md", "[x](nope.md) /home/alice\n")
        self.write("docs/ai/plans/draft.md", "[x](nope.md) /home/alice\n")
        self.write("docs/dev/notes.local.md", "[x](nope.md)\n")
        self.assertEqual(self.problems(), [])

    def test_main_returns_nonzero_on_problems(self):
        self.write("AGENTS.md", "[x](nope.md)\n")
        self.assertEqual(check_docs.main(["--root", self.root, "--quiet"]), 1)
        self.write("AGENTS.md", "fine\n")
        self.assertEqual(check_docs.main(["--root", self.root, "--quiet"]), 0)


if __name__ == "__main__":
    unittest.main()
