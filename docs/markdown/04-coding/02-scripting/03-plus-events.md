# OpenMOHAA Plus script events

Events added by OpenMOHAA Plus on top of the [OpenMoHAA script events](01-script-events.md). Subscribe
with `event_subscribe` as usual. Events marked *deciding* fire **before** the action and read the
handler's return value (`end <value>`); a handler that waits (`wait`, `waitframe`) returns nothing,
which means "no change". With several subscribers, the last handler's value counts.

## Player events (`self` is the player)

### player_damage (deciding)

The player is about to be damaged. Same parameters as `player_damaged`:

```
player_damage local.attacker local.damage local.inflictor local.position local.direction local.normal local.knockback local.damageflags local.meansofdeath local.location
```

Return:
- a number: replaces the damage (before hit-location multipliers);
- `0` or less: no damage at all (no knockback either);
- nothing: damage unchanged.

```
main:
    event_subscribe "player_damage" no_falling_damage
end

no_falling_damage local.attacker local.damage local.inflictor local.position local.direction local.normal local.knockback local.damageflags local.meansofdeath:
    if (local.meansofdeath == 7) // MOD_FALLING (meansOfDeath_t in code/fgame/bg_public.h)
        end 0
end
```

### player_teamchange (deciding)

The player asked to join a team (`join_team`, team menu, auto-join). Not fired when the game restores
teams after a round restart or for the `setteam` script command.

```
player_teamchange local.new_team local.old_team
```

Team names: `allies`, `axis`, `freeforall`, `spectator`, `none`. Return `0` to refuse, or a string
to refuse and print it to the player.

## Level events

Fired in round-based gametypes (round-based match, objective, Tug-of-War, Liberation).

| Event | Parameters | When |
|---|---|---|
| `match_start` | none | the first round of the map starts |
| `round_start` | `local.round` | a round starts (1 for the first one) |
| `round_end` | `local.winner local.axis_wins local.allies_wins` | the round winner is known (`axis`, `allies` or `draw`); win counts include this round |

## Control commands

Commands that let a script drive the match, together with the existing `teamwin`, `team_setscore`,
`join_team` and `respawn`.

| Command | Effect |
|---|---|
| `team_swap` | Halftime side swap: every axis player becomes allies and vice versa; the team scores (wins, kills, deaths) follow the players. Living players respawn at their new side. Team games only. Does not fire `player_teamchange`. |
| `match_pause` / `match_resume` | Freezes the match: players frozen, no damage, round clock and time limit stopped. `level.match_paused` is 1 while paused. See [match control](../../03-configuration/08-plus-match-control.md). |
| `team_getscore "axis"` | Returns the team score (round wins in round-based modes). Fixed in OpenMOHAA Plus: the value could not be read before. |

In round-based modes the map script runs again every round, so code in `main` runs once per round.
A halftime swap in a round-based mode:

```
main:
	event_subscribe "round_end" on_round_end
end

on_round_end local.winner local.axis_wins local.allies_wins:
	if (local.axis_wins + local.allies_wins == 6) {
		team_swap
	}
end
```
