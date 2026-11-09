# OpenMOHAA Plus: in-game administration

Admin accounts let trusted players run server commands from the game without the RCON password.
Each account gets only the permissions it needs. RCON keeps working as before.

## Setup

1. Create accounts from the server console (passwords are stored hashed with PBKDF2-SHA256, never
   in clear):

   ```
   admin_add vwz <password>
   ```

2. Give them permissions in `main/admin_config.txt` (next to `serverbans.dat` in the server's home
   folder), then restart the server:

   ```
   // roles are reusable sets of permissions
   role moderator allow admin.status admin.kick admin.clientkick admin.ban* cvar.timelimit cvar.fraglimit
   role moderator deny admin.unban

   account vwz roles moderator
   account vwz allow admin.map admin.restart
   account vwz deny cvar.fraglimit
   ```

   A permission is an exact name, a name ending in `*` (`admin.ban*` covers `admin.ban`,
   `admin.banfoo`...) or `*` for everything. **Deny always wins** over allow, whether it comes from a
   role or the account. A new account has no permissions until it appears in this file.

3. In the game, open the console: `ad_login vwz <password>`, then `ad_help`.

## Player commands

| Command | Permission | Runs |
|---|---|---|
| `ad_login <account> <password>`, `ad_logout`, `ad_help` | none | |
| `ad_status` | `admin.status` | `status` |
| `ad_kick <player name>` | `admin.kick` | `kick` |
| `ad_clientkick <client number> [reason]` | `admin.clientkick` | `clientkick` |
| `ad_ban <ip[/subnet] or client number> [reason]` | `admin.ban` | `banaddr` |
| `ad_unban <ip[/subnet] or ban number>` | `admin.unban` | `bandel` |
| `ad_listbans` | `admin.listbans` | `listbans` |
| `ad_map <map>` | `admin.map` | `map` |
| `ad_restart` | `admin.restart` | `restart` |
| `ad_pause` / `ad_resume` | `admin.pause` | `matchpause` / `matchresume` |
| `ad_say <message>` / `ad_tell <client number> <message>` | `admin.say` / `admin.tell` | `say` / `tell` |
| `ad_dumpuser <player name>` | `admin.dumpuser` | `dumpuser` |
| `ad_set <cvar> <value>` | `cvar.<name>` (for example `cvar.timelimit`, `cvar.*`) | sets the cvar |
| `ad_rcon <command> [arguments]` | `admin.rcon` | any console command |

- `admin.rcon` means full trust: it runs any console command. Password cvars (`rconPassword`,
  `sv_privatePassword`) and `sv_banFile` also need `admin.rcon`, even with `cvar.*`.
- Arguments cannot contain `;` or quotes, so one admin command runs exactly one server command.
- The command output is sent to the admin's console. Level changes (`map`, `restart`) run at the end
  of the frame.

## Server console commands

| Command | Effect |
|---|---|
| `admin_add <account> <password>` | creates an enabled account without permissions |
| `admin_passwd <account> <password>` | changes the password |
| `admin_enable <account>` / `admin_disable <account>` | enables or disables the account |
| `admin_del <account>` | deletes the account |
| `admin_list` | lists accounts and who is logged in |

Changing the password, disabling or deleting an account ends its session at once. Account changes
apply immediately; `admin_config.txt` is read at server start only.

## Sessions and security

- A session belongs to one connection: it ends on logout or disconnect, and an account can be
  logged in from one client at a time.
- After a failed login the address waits 2 seconds; after 5 failures, 60 seconds. The server checks
  at most one password per second, since each check takes a few tens of milliseconds.
- Every login, admin command and account change is appended to `main/admin.log` (time, address,
  player name, account, result). Passwords are never written to logs.
- Like RCON, the game protocol is not encrypted: the password travels in clear over the network.
  Use a password that is not used anywhere else.
