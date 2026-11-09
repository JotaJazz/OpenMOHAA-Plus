#!/usr/bin/env python3
"""Check the OpenMOHAA Plus technical docs (AGENTS.md, LAYOUT.md, docs/dev, docs/ai).

Flags broken relative links, repository paths in backticks that git would not commit
(tracked or untracked-but-not-ignored; plain filesystem check outside a git repo),
personal data (home/mount/drive paths, non-documentation IPs, emails, tokens) and
PROJECT_MEMORY stages that are malformed or have more than 3 bullets.
Upstream user docs (docs/markdown) and local plans are not checked.

Usage: python3 misc/docs-check/check_docs.py [--root DIR] [--quiet]
"""

import argparse
import ipaddress
import os
import re
import subprocess
import sys
from dataclasses import dataclass

CHECKED_FILES = ["AGENTS.md", "LAYOUT.md"]
CHECKED_DIRS = ["docs/dev", "docs/ai"]
EXCLUDED_DIRS = {"plans"}
EXCLUDED_SUFFIX = ".local.md"
MEMORY_FILE = "docs/ai/PROJECT_MEMORY.md"
MAX_STAGE_BULLETS = 3

# Top-level entries a backticked path must start with to be checked.
PATH_ROOTS = ("code/", "docs/", "cmake/", "misc/", "container/", ".github/")

LINK_RE = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")
BACKTICK_RE = re.compile(r"`([^`\n]+)`")
STAGE_RE = re.compile(r"^## \d{4}-\d{2}-\d{2} — \S")
IPV4_RE = re.compile(r"(?<![\w.])(\d{1,3}(?:\.\d{1,3}){3})(?!\w|\.\d)")
ALLOWED_NETS = [ipaddress.ip_network(n) for n in
                ("192.0.2.0/24", "198.51.100.0/24", "203.0.113.0/24",
                 "127.0.0.0/8", "0.0.0.0/32")]
PERSONAL_RES = [
    (re.compile(r"/home/[A-Za-z0-9_.-]+"), "home directory path"),
    (re.compile(r"/Users/[A-Za-z0-9_.-]+"), "home directory path"),
    (re.compile(r"/media/[A-Za-z0-9_.-]+"), "mounted media path"),
    (re.compile(r"/mnt/[a-z](?:/|\b)"), "mounted drive path"),
    (re.compile(r"(?<![A-Za-z0-9])[A-Za-z]:[\\/]+[A-Za-z]"), "Windows drive path"),
    (re.compile(r"\\\\wsl", re.IGNORECASE), "WSL share path"),
    (re.compile(r"\bgh[opsu]_[A-Za-z0-9]{36}\b"), "GitHub token"),
    (re.compile(r"\bgithub_pat_[A-Za-z0-9_]{22,}"), "GitHub token"),
    (re.compile(r"\b(?:sk|pk)-[A-Za-z0-9]{20,}\b"), "API key"),
    (re.compile(r"\bAKIA[0-9A-Z]{16}\b"), "AWS key"),
    (re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----"), "private key"),
]


EMAIL_RE = re.compile(r"[\w.+-]+@[\w-]+(?:\.[\w-]+)+")


def allowed_email(addr):
    local, domain = addr.lower().split("@", 1)
    return "noreply" in local or "noreply" in domain


@dataclass
class Problem:
    path: str
    line: int
    kind: str
    message: str

    def __str__(self):
        return f"{self.path}:{self.line}: {self.kind}: {self.message}"


def doc_files(root):
    files = [f for f in CHECKED_FILES if os.path.isfile(os.path.join(root, f))]
    for d in CHECKED_DIRS:
        for dirpath, dirnames, filenames in os.walk(os.path.join(root, d)):
            dirnames[:] = sorted(n for n in dirnames if n not in EXCLUDED_DIRS)
            for name in sorted(filenames):
                if name.endswith(".md") and not name.endswith(EXCLUDED_SUFFIX):
                    files.append(os.path.relpath(os.path.join(dirpath, name), root))
    return [f.replace(os.sep, "/") for f in files]


def check_links(root, rel, lineno, line):
    for target in LINK_RE.findall(line):
        if re.match(r"^[a-z][a-z0-9+.-]*:", target) or target.startswith("#"):
            continue
        target = target.split("#", 1)[0]
        resolved = os.path.normpath(os.path.join(root, os.path.dirname(rel), target))
        if not os.path.exists(resolved):
            yield Problem(rel, lineno, "broken-link", target)


def repo_paths(root):
    """Paths git would commit (tracked + untracked, not ignored) and their parent dirs.

    Returns None outside a git work tree, so callers fall back to the filesystem.
    """
    try:
        out = subprocess.run(["git", "-C", root, "ls-files", "-co", "--exclude-standard"],
                             capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    paths = set()
    for f in out.splitlines():
        parts = f.split("/")
        for i in range(1, len(parts) + 1):
            paths.add("/".join(parts[:i]))
    return paths


def check_paths(root, rel, lineno, line, known=None):
    for token in BACKTICK_RE.findall(line):
        token = token.strip()
        if not token.startswith(PATH_ROOTS) or " " in token:
            continue
        if any(c in token for c in "<>*{}$"):
            continue
        path = re.sub(r":\d+(?:-\d+)?$", "", token).rstrip("/")
        exists = path in known if known is not None else os.path.exists(os.path.join(root, path))
        if not exists:
            yield Problem(rel, lineno, "missing-path", token)


def check_personal(rel, lineno, line):
    for regex, what in PERSONAL_RES:
        for m in regex.finditer(line):
            yield Problem(rel, lineno, "personal-data", f"{what}: {m.group(0)}")
    for m in EMAIL_RE.finditer(line):
        if not allowed_email(m.group(0)):
            yield Problem(rel, lineno, "personal-data", f"email address: {m.group(0)}")
    for m in IPV4_RE.finditer(line):
        try:
            ip = ipaddress.ip_address(m.group(1))
        except ValueError:
            continue
        if not any(ip in net for net in ALLOWED_NETS):
            yield Problem(rel, lineno, "personal-data", f"IP address: {m.group(1)}")


def check_memory(rel, lines):
    stage, bullets, stage_line = None, 0, 0
    for lineno, line in enumerate(lines, 1):
        if line.startswith("## "):
            if stage and bullets > MAX_STAGE_BULLETS:
                yield Problem(rel, stage_line, "memory-stage", f"{bullets} bullets (max {MAX_STAGE_BULLETS})")
            if not STAGE_RE.match(line):
                yield Problem(rel, lineno, "memory-stage", "heading must be '## YYYY-MM-DD — topic'")
            stage, bullets, stage_line = line, 0, lineno
        elif stage and line.startswith("- "):
            bullets += 1
    if stage and bullets > MAX_STAGE_BULLETS:
        yield Problem(rel, stage_line, "memory-stage", f"{bullets} bullets (max {MAX_STAGE_BULLETS})")


def check_file(root, rel, known=None):
    with open(os.path.join(root, rel), encoding="utf-8") as f:
        lines = f.read().splitlines()
    problems, in_fence = [], False
    for lineno, line in enumerate(lines, 1):
        problems.extend(check_personal(rel, lineno, line))
        if line.lstrip().startswith(("```", "~~~")):
            in_fence = not in_fence
            continue
        if not in_fence:
            problems.extend(check_links(root, rel, lineno, line))
            problems.extend(check_paths(root, rel, lineno, line, known))
    if rel == MEMORY_FILE:
        problems.extend(check_memory(rel, lines))
    return problems


def check_repo(root):
    problems, known = [], repo_paths(root)
    for rel in doc_files(root):
        problems.extend(check_file(root, rel, known))
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)
    problems = check_repo(os.path.abspath(args.root))
    if not args.quiet:
        for p in problems:
            print(p)
        print(f"{len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
