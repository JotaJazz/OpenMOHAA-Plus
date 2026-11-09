# Fork workflow

Policy: [ADR 0001](../architecture/adr/0001-fork-keeps-ai-assisted-work.md),
[ADR 0003](../architecture/adr/0003-one-branch-per-issue.md).

## Remotes

| Remote | Repository | Use |
|---|---|---|
| `origin` | `JotaJazz/OpenMOHAA-Plus` | push branches and `main` |
| `upstream` | `openmoh/openmohaa` | read-only source of upstream changes and issues; push disabled with `git remote set-url --push upstream DISABLED` |

## Sync with upstream

```sh
git fetch upstream
git switch main && git merge upstream/main     # resolve conflicts, build, smoke test
git push origin main
```

## Fixing an upstream issue

New features follow the same branch flow; check [ideas.md](../roadmap/ideas.md) first.

1. Check the issue has no open upstream PR (`gh pr list -R openmoh/openmohaa --search <topic>`).
2. `git switch -c fix/<topic> main`.
3. Reproduce headless, fix the root cause, reproduce again ([testing.md](testing.md)).
4. Commit (format below), `git push -u origin fix/<topic>`.
5. Merge batches into `main` with `--no-ff`, smoke test, push, delete merged branches
   (`git push origin --delete fix/<topic>`, `git branch -d fix/<topic>`).
6. On `main`, update `docs/ai/SYSTEM_SUMMARY.md`, add one `docs/ai/PROJECT_MEMORY.md` stage for the
   batch, and record rejected or open items in `docs/dev/roadmap/debt.md`.

## Commit message

```
<type>(<area>): <imperative summary>          # type: fix, feat, ci, docs, refactor; area: client, server, fgame, qcommon, sdl...

What was wrong and why (openmoh/openmohaa#N).
What the change does.
How it was verified: command/scenario, observed before vs after.
Behaviour changes and what was not checked.

Co-Authored-By: <assistant trailer>
```

Never send these commits, or text written by an assistant, to the upstream repository.
