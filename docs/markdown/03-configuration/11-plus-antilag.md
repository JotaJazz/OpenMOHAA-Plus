# OpenMOHAA Plus: lag compensation

A player sees the others where they were a ping and an interpolation delay ago. With lag
compensation, when a player fires a bullet weapon the server moves every other player back to
that moment (position, angles and animation pose, which decide the hit location), traces the
bullets and puts everyone back. Aiming at the enemy on screen hits, whatever the ping.

| Cvar | Default | Effect |
|---|---|---|
| `g_antilag` | `0` | `1`: lag compensation for bullet weapons (rifles, SMGs, MGs, shotguns, mounted guns). Shown in the server browser info. Off keeps the original behaviour (leading targets by your ping). |
| `g_antilag_maxms` | `300` | Players are never moved back more than this many milliseconds: a very high ping no longer gets compensated in full, so players with a good connection are not shot "behind cover" too often. |
| `g_antilag_debug` | `0` | `1`: log every rewind, with how far sideways the shooter's aim passed from the rewound target and from where it is now. |

Not compensated: grenades, rockets and other projectiles (they fly in the server's present), melee,
and bots' shots (bots see the present). Being shot just after reaching cover is the known trade-off of
lag compensation.
