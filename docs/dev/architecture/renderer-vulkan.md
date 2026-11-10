# Vulkan renderer (renderer_vulkan)

Decision: [ADR 0005](adr/0005-vulkan-renderer-emulates-gl1.md). User page:
[12-plus-vulkan.md](../../markdown/03-configuration/12-plus-vulkan.md).

## Shape

`renderer_vulkan` is the GL1 renderer (`code/renderergl1`, unchanged) built with `USE_VULKAN`
against an emulation of the OpenGL 1.x calls it makes. Every MOHAA feature of GL1 (TIKI models,
terrain, sky portals, static models, marks, fonts) comes with it; a fix to GL1 fixes both.

| File | Role |
|---|---|
| `code/sdl/sdl_glimp.c` (`USE_VULKAN` blocks) | Window without GL context, Vulkan device, `qgl*` set to the emulation, `VK_EndFrame` in place of the buffer swap; when Vulkan cannot start, the normal OpenGL context path (same renderer on real GL) |
| `code/renderervk/vk_gl.c` | GL state (enables, blend, depth, stencil, matrices, texture units and environments, client arrays, `glBegin`/`glEnd`), primitives turned into triangle/line/point lists, textures (format conversion, S3TC decoded on the CPU), `glReadPixels` |
| `code/renderervk/vk_core.c` | Instance and device (loader loaded at run time), offscreen color + depth/stencil image the size of the video mode, swapchain blit, pipelines keyed by the state that Vulkan bakes in, push descriptors, per-frame rings, texture images, samplers |
| `code/renderervk/shaders/` | One vertex and one fragment shader interpret the per-draw state: texture environments including `GL_COMBINE`/`GL_COMBINE4_NV`, alpha test, linear/exp fog, clip plane. `compile.py` writes `vk_shaders.h` (committed) |
| `cmake/renderer_vulkan.cmake` | Built when the Vulkan headers are found (`BUILD_RENDERER_VULKAN`, default ON) |

## Contracts

- GL coordinates kept: the vertex shader flips y and maps depth to 0..1, so viewports and scissors
  are flipped to a top-left origin and front faces stay counter-clockwise.
- OpenGL never clears on its own: the render pass loads, `glClear` becomes `vkCmdClearAttachments`.
- Texture uploads and `glReadPixels` are recorded in order with the draws (the render pass is ended
  and resumed around them), so GL ordering holds.
- Headless (SDL `offscreen` driver, no Vulkan window): no swapchain; frames stay in the offscreen
  image that screenshots read. This is how the tests run (Mesa lavapipe, no GPU).

## Not emulated

`glDrawPixels`, display lists, texture matrices, `glCopyTexSubImage2D`, line stipple (all unused or
debug-only in GL1), partial color masks on `glClear`. Unknown uses log once with `developer 1`.
Limits and performance shortcuts: [debt.md](../roadmap/debt.md).
