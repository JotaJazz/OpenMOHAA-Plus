# OpenMOHAA Plus: bot skill, weapons and grenades

| Cvar | Default | Effect |
|---|---|---|
| `g_bot_skill` | `3` | `1` Novice, `2` Easy, `3` Average, `4` Hard, `5` Expert. Scales the bots' reaction time, aim spread and turn speed; takes effect at once. The server browser shows the level. |
| `g_bot_grenades` | `1` | Bots throw grenades now and then (about every 12 to 20 seconds of fighting) at enemies 256 to 1100 units away. With grenades as their only weapon (gun game mods) they throw them like any weapon. `0`: never. |
| `g_bot_weapons` | empty | Primary weapons bots choose from, at random: any of `rifle sniper smg mg heavy shotgun` (e.g. `"rifle smg"`). Empty: automatic choice, as before. Applies to bots that join after the change. |

The levels multiply the detailed cvars, which stay available for fine tuning: Average is exactly
`g_bot_attack_react_min_delay`, `g_bot_attack_react_random_delay`, `g_bot_attack_spreadmult` and
`g_bot_turn_speed`.

| Level | Reaction time | Aim spread | Turn speed |
|---|---|---|---|
| Novice | x2.0 | x2.0 | x0.6 |
| Easy | x1.5 | x1.5 | x0.8 |
| Average | x1.0 | x1.0 | x1.0 |
| Hard | x0.6 | x0.6 | x1.3 |
| Expert | x0.3 | x0.3 | x1.8 |

Scripts can still give a single bot another weapon (`local.bot primarydmweapon smg`).
