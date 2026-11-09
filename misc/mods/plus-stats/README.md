# plus-stats (reference server mod)

Posts per-round statistics (map, winner, team wins, kills per player) to a web service, using only
scripts: `sv_scripts`, the `player_killed`/`round_end` events and `create_api_request`. Setup is at
the top of `global/plus/stats.scr`. Round-based gametypes only (rounds end with `round_end`).
Tested by `ReferenceMods` in `tests/test_server.py`.
