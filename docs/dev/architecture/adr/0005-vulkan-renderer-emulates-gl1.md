# 0005. The Vulkan renderer emulates the GL1 renderer's OpenGL calls

- Status: Accepted
- Date: 2026-11-09

## Context
A Vulkan renderer was planned (phase 5). The GL1 renderer is about 40 000 lines with MOHAA-specific
rendering (TIKI models, terrain, sky portals, static models); only its backend talks to OpenGL, through
76 distinct `qgl*` calls, almost all OpenGL 1.x fixed function. Quake3e's Vulkan renderer (GPL-2.0)
shows the alternative: a renderer copy with Vulkan calls woven into its backend.

## Decision
Build `renderer_vulkan` from the unchanged GL1 sources plus an emulation of those `qgl*` calls on
Vulkan (`code/renderervk`), with the window code in `code/sdl/sdl_glimp.c` under `USE_VULKAN`. GL
state becomes pipelines (blend, depth, cull, stencil) and a per-draw uniform block interpreted by one
pair of shaders. Device setup follows Quake3e's; no Quake3e code is copied.

## Consequences
- No second copy of the renderer: upstream fixes to GL1 reach both renderers, and screenshots of
  the two can be compared pixel by pixel (the tests do).
- Performance is bounded by GL1's draw structure (many small draws, state per draw); a faster
  Vulkan path would change the backend itself later.
- The emulation must follow every new GL call GL1 starts making; unemulated calls log once.
