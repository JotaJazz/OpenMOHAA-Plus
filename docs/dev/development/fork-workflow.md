# Fork workflow

Policy: [ADR 0001](../architecture/adr/0001-fork-keeps-ai-assisted-work.md),
[ADR 0003](../architecture/adr/0003-one-branch-per-issue.md).

## Remotes

| Remote | Repository | Use |
|---|---|---|
| `origin` | `OpenGamePlus/OpenMOHAA-Plus` | push branches and `main` |
| `upstream` | `openmoh/openmohaa` | read-only source of upstream changes and issues; push disabled with `git remote set-url --push upstream DISABLED` |

## Sync with upstream

```sh
git fetch upstream
git switch main && git merge upstream/main     # resolve conflicts, build, smoke test
git push origin main
```

Automated: `.github/workflows/upstream-sync.yml` runs weekly (and on demand from the Actions tab),
merges `upstream/main` into a branch `upstream-sync/<date>` and opens a pull request inside the
fork; on conflicts the job fails and lists the files. GitHub disables scheduled workflows in forks
until they are enabled in the repository's Actions tab.

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

## Versions and releases

- Version: upstream `PROJECT_VERSION` plus a fork revision, shown as `X.Y.Z-plus.N`
  (`PRODUCT_VERSION_PLUS` in `cmake/identity.cmake`). Reset `N` to 1 when an upstream merge bumps
  `PROJECT_VERSION`.
- Release: bump `PRODUCT_VERSION_PLUS`, merge to `main`, push a tag `vX.Y.Z-plus.N`; the
  `.github/workflows/tags-publish-release.yml` workflow builds and publishes it. Only the
  maintainer publishes releases.
- The in-game update checker (`code/sys/sys_update_checker.cpp`) reads the fork's latest release
  and understands `vX.Y.Z` and `vX.Y.Z-plus.N` tags.

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
