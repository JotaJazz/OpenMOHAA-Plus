# OpenMOHAA Plus: server browser favorites, history and filters

## Favorites

- In the internet server list, select a server and press **F** to add it to or remove it from your
  favorites. Favorites stay at the top of the list.
- From the console: `fav_add` (the server you are playing on), `fav_add <ip:port> [query port]`,
  `fav_del [ip:port]`, `fav_list`.

## History

The last 20 servers you joined are remembered; they show in the list as `(recent)`. Console:
`history_list`, `history_clear`.

Favorite and recent servers are queried directly every time the list is refreshed, so they show up
even when the master servers are down or do not list them.

## Filters

| Cvar | Effect |
|---|---|
| `dm_browser_show` | `0` all servers, `1` favorites only, `2` favorites and recent servers (`1` and `2` do not ask the master servers) |
| `dm_browser_search` | show only servers whose name or map contains this text |
| `dm_max_ping` | hide servers with a higher ping (`0` = no limit) |
| `dm_filter_empty`, `dm_filter_full` | hide empty / full servers (existing) |
| `dm_min_players`, `dm_max_players` | player count range (existing) |

Favorites are never hidden by the filters. Change a filter, then press Refresh.

## Files

`serverfavorites.txt` and `serverhistory.txt` in the configuration folder, one server per line:
`<ip>:<port> <query port>`. The query port is the server's GameSpy port (`net_gamespy_port`,
12300 by default for game port 12203). OpenMOHAA Plus servers publish it; for other servers it is
taken from the browser, or guessed as the game port + 97.
