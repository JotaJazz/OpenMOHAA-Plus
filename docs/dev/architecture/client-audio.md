# Client: audio and platform notes

## Sound path

```
cgame / UI ── S_StartSound (code/client/snd_dma_new.cpp)
               │  applies s_dialogscale to CHAN_DIALOG*, alias lookup (code/qcommon/alias.c)
               ▼
             S_OPENAL_* (code/client/snd_openal_new.cpp)  ── OpenAL Soft (dlopen)
               │  2D channels, 3D channels, streamed channels, loop sounds
             S_LoadSound (code/client/snd_mem_new.cpp)     ── WAV/MP3 from pk3
```

| Fact | Where | Consequence |
|---|---|---|
| Each source gets `AL_MAX_GAIN` = `AL_GAIN_LIMIT_SOFT` when `AL_SOFT_gain_clamp_ex` exists | `S_OPENAL_InitChannel` | Volumes above 1.0 (`s_dialogscale > 1`, aliases with volume 1.5) are audible; default OpenAL clamps to 1.0 |
| The OpenAL output limiter is disabled | `ALC_OUTPUT_LIMITER_SOFT` in `snd_openal_new.cpp` | Gains above full scale clip instead of being compressed |
| Final gain = `s_volume` × channel volume × multipliers | `S_GetBaseVolume` | Many stock aliases already request 1.5 |
| Names without extension get `.wav` | `S_LoadSound` | Stock `ubersound` has extensionless aliases (e.g. `impact_playerleavewater`) |
| Channel start/end times are saved relative and fixed up on load | `S_SaveBase` / `S_InitBase` | Same fixup list as game times (`SV_FinishSvsTimeFixups`) |

Verifying audio without speakers: OpenAL Soft's wave backend renders to a file; see
[testing.md](../development/testing.md).

## Client startup and console

- With `developer 0`, `CL_TryStartIntro` (`code/client/cl_ui.cpp`) plays the intro on every
  start (`cl_playintro` is not archived); console commands wait until it ends.
- The console key opens the console only with `ui_console 1` (`code/client/cl_keys.cpp`).
- Non-interactive stdin is a console when `ttycon 0` (`code/sys/con_tty.c`); used by tests.

## Platform layer

- Cursor images: `IN_SetCursorFromImage` (`code/sdl/sdl_mouse.c`) frees the image right after
  creating the SDL cursor; the free function belongs to the renderer, which `vid_restart` reloads.
- `vid_restart` reloads the renderer module; any function pointer obtained from it is invalid
  afterwards.
