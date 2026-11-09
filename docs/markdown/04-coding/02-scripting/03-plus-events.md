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
