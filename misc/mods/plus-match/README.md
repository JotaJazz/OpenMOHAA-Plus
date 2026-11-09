# plus-match: competition match rules (reference mod)

A zPAM-like match mode written only in scripts, on top of the OpenMOHAA Plus gametype API.

## Install

Put the `global/` folder and `plus-match.cfg` in the server's `main/` folder (or pack `global/`
in a pk3), then `exec plus-match.cfg`. Edit the config for your league; the important settings:

| Cvar | Example | Meaning |
|---|---|---|
| `sv_scripts` | `global/plus/match.scr` | runs the mod on every level |
| `plus_match_rounds` | `12` | rounds in the match: first to 7 wins, sides switch after 6 |
| `plus_match_timeouts` | `1` | timeouts per team and half |
| `plus_match_timeout_length` | `60` | seconds |
| `g_forceready` | `0` | players type `ready` before the first round (built into the game) |

Use a round-based game type (objective, round-based team match).

## During the match

- The score is announced after every round.
- Halftime: after `plus_match_rounds / 2` rounds both teams switch sides (`team_swap`): every player
  changes team and the round wins follow the players. Timeouts are reset.
- A team wins when it reaches `plus_match_rounds / 2 + 1` round wins; if all rounds are played with
  a tied score the match ends in a draw.
- Players type `timeout`: the match pauses (everyone frozen, clock stopped) for
  `plus_match_timeout_length` seconds or until their team types `timein`.
- Admins: `timeout axis|allies` and `timein` in the server console or with `ad_rcon`, or
  `ad_pause` / `ad_resume`.

## How it works

It uses `match_start`, `round_end`, `team_swap`, `match_pause` / `match_resume`, `registercmd` and
`fraglimit`. Round-based modes restart the map every round, so the match state is kept in cvars
(`plus_match_timeouts_axis`, `plus_match_timeouts_allies`, `plus_match_timeout_team`).
