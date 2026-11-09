# 0001. AI-assisted work stays in the fork

- Status: Accepted
- Date: 2026-11-08

## Context
OpenMOHAA Plus is a fork of `openmoh/openmohaa`. Upstream `CONTRIBUTING.md` forbids code, commits,
issues and pull requests generated with AI or LLM tools. Work in this fork is AI-assisted.

## Decision
AI-assisted changes are committed only to this fork and never submitted upstream (no PRs, issues
or comments written by an assistant). Commits keep a `Co-Authored-By` trailer that names the
assistant. Upstream changes flow one way: `upstream/main` is merged into the fork's `main`.

## Consequences
- The fork diverges; keep changes small and localized to ease merges from upstream.
- Upstream files (user docs, `README.md`, `CONTRIBUTING.md`, existing workflows) are left
  unchanged where possible; fork-only material lives in new files (`docs/dev`, `docs/ai`,
  `AGENTS.md`, `misc/docs-check`, `.github/workflows/docs-check.yml`).
- Issue numbers in commit messages refer to upstream issues as `openmoh/openmohaa#N`.
