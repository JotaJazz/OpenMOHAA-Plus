# Architecture decision records

An accepted ADR is never edited; a later ADR supersedes it and the old one gets
`Status: Superseded by NNNN`. Number sequentially, file name `NNNN-short-title.md`.

| ADR | Title | Status |
|---|---|---|
| [0001](0001-fork-keeps-ai-assisted-work.md) | AI-assisted work stays in the fork | Accepted |
| [0002](0002-original-behaviour-by-default.md) | Original-game behaviour by default | Accepted |
| [0003](0003-one-branch-per-issue.md) | One branch per upstream issue, merged with --no-ff | Accepted |
| [0004](0004-module-may-stay-resident.md) | A game module may stay resident after unload | Accepted |
| [0005](0005-vulkan-renderer-emulates-gl1.md) | The Vulkan renderer emulates the GL1 renderer's OpenGL calls | Accepted |
| [0006](0006-fork-licensed-agpl-3.md) | The fork is distributed under AGPL-3.0 | Accepted |

## Template (MADR-lite)

```markdown
# NNNN. Title

- Status: Proposed | Accepted | Superseded by NNNN
- Date: YYYY-MM-DD

## Context
Forces, constraints, what triggered the decision.

## Decision
What was decided, in one or two short paragraphs.

## Consequences
What becomes easier, harder, or must now be maintained.
```
