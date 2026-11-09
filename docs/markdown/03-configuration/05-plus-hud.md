# OpenMOHAA Plus: HUD and crosshair size

| Cvar | Default | Effect |
|---|---|---|
| `ui_hudscale` | `1` | Multiplies the HUD size (health, ammo, compass, score, messages, stopwatch). From `0.5` to `4`. Applies after `vid_restart`. |
| `cg_crosshair_size` | `0` | Crosshair size in HUD units, whatever the texture resolution. `0` keeps the texture's own size (original behaviour). |
| `cg_crosshair_scale` | `1` | Multiplies the crosshair size. |

The HUD is drawn at its original pixel size up to 1920x1080 and scaled with the screen height above
that, so at 1920x1080 it is as small as at 640x480. `ui_hudscale 1.5` or `2` makes it readable there:

```
seta ui_hudscale 2
vid_restart
```

Menus are not affected by `ui_hudscale`. The field of view already keeps the vertical angle on wide
screens (Hor+): `fov` is the horizontal angle on a 4:3 screen.
