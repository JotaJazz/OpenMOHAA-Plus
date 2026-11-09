# OpenMOHAA Plus: match pause, hit sounds and hit markers

## Match pause

| Where | Pause | Resume |
|---|---|---|
| Server console / RCON | `matchpause` | `matchresume` |
| In-game admin ([admin accounts](04-plus-admin.md), permission `admin.pause`) | `ad_pause` | `ad_resume` |
| Scripts | `match_pause` | `match_resume` |

While paused, every player (bots included) is frozen and takes no damage, and the clock stands
still: the round timer in round-based modes and the time limit otherwise (the HUD clock stops too).
Players see "Match paused" / "Match resumed". Scripts can read `level.match_paused` (1 while paused).
Projectiles already in flight are not stopped. A new map starts unpaused.

## Hit sounds

| Cvar | Default | Effect |
|---|---|---|
| `g_hitsounds` | `0` | `1`: a player hears a sound each time they damage an enemy (at most every 50 ms) |
| `g_hitsound` | `sound/items/hit_notify.wav` | the sound file, from the game's own data or a pk3 the clients have |

The server decides, so every player on the server gets the same help. The sound is played with the
client's `play` command, so original MOHAA clients hear it too. Hits on teammates make no sound.

## Hit markers

| Cvar | Where | Default | Effect |
|---|---|---|---|
| `g_hitmarkers` | server | `0` | `1`: a small X flashes around the crosshair when a player damages an enemy |
| `cg_hitmarker` | player | `1` | `0`: the player does not want them |

Only OpenMOHAA Plus clients draw hit markers; they tell the server through their userinfo, so
original clients never receive the command. Like hit sounds, the server decides, and teammate hits
show nothing.
