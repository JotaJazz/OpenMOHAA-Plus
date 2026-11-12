# OpenMOHAA Plus: Vulkan renderer (experimental)

`renderer_vulkan` draws the game with Vulkan instead of OpenGL. It is the same renderer as the
default one (`opengl1`) on top of Vulkan, so the picture is the same; it is useful on systems whose
OpenGL drivers are poor or missing.

```
+set cl_renderer vulkan
```

or `cl_renderer vulkan` and `vid_restart` in the console; `cl_renderer opengl1` goes back. If
Vulkan cannot start (no Vulkan driver), the same renderer runs on OpenGL instead and the console
says `Vulkan could not start, using OpenGL`.

| Cvar | Effect |
|---|---|
| `r_swapInterval` | `1`: vertical sync (FIFO presentation); `0`: no sync |

State: tested headless with Mesa's software Vulkan driver on Linux (menus, maps, models, sky, fog
match the OpenGL renderer pixel for pixel), and in a window on an NVIDIA GPU (proprietary driver,
X11, and Windows). The Linux and Windows builds include it; macOS does not yet.
Report problems with your GPU and driver.
