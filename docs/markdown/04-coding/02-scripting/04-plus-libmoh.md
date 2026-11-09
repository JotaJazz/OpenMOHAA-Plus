# OpenMOHAA Plus script functions (libmoh)

Functions added by OpenMOHAA Plus. Names, arguments and return values follow NightFall's functions of
the same names, so scripts written for NightFall run unchanged.

## create_api_request

```
local.result = create_api_request <url> <method> <handler> [userdata]
```

Queues an HTTP request. Needs the server cvar `sv_api_client 1` (off by default).

| Argument | Meaning |
|---|---|
| url | `http://` or `https://` address; other protocols fail with code -1 |
| method | `"get"` or `"post"` |
| handler | label to call when the request finishes, e.g. `global/api.scr::on_response` |
| userdata | any value, passed back to the handler; for `post` it is also sent as the body, as typed JSON (see `json_stringify`) |

Returns `0` queued, `1` invalid arguments, `2` API client not running (`sv_api_client 0`), `3` error.

The handler is called on a later server frame, at most one response per frame:

```
on_response local.http_code local.response local.userdata:
    if (local.http_code == -1)
    {
        println ("request failed: " + local.response)
        end
    }
    local.data = json_parse local.response
end
```

`local.http_code` is the HTTP status, or `-1` with the error text in `local.response`. Requests time
out after 20 seconds, responses are limited to 1 MiB, and pending requests are dropped when the map
changes.

## json_parse

```
local.var = json_parse <string>
```

Objects become arrays indexed by key, JSON arrays become arrays indexed from 0, `true`/`false` become
1/0, `null` becomes NULL. Invalid JSON raises a script error.

## json_stringify

```
local.text = json_stringify <value>
```

Returns NightFall's typed JSON: every value is `{"type": ..., "content": ...}` with types `string`,
`integer`, `float`, `char`, `vector`, `array` (constant arrays) and `array_object` (other arrays).
Entities and other references cannot be converted.
