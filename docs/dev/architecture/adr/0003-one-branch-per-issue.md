# 0003. One branch per upstream issue, merged with --no-ff

- Status: Accepted
- Date: 2026-11-09

## Context
Fixes target independent upstream issues and must be reviewable and revertible one by one.

## Decision
Each fix gets a branch `fix/<topic>`, one commit (or a few) with the issue reference and how it was
verified, pushed to `origin`. After review, branches are merged into `main` with `--no-ff` and the
branch is deleted locally and on the remote.

## Consequences
- `git log --first-parent main` lists one merge per fix; `git revert -m 1 <merge>` undoes one fix.
- Before merging a batch, build the merged `main` and run the headless smoke tests in
  `docs/dev/development/testing.md`.
