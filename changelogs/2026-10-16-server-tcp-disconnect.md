# Disconnect detected when the TCP connection closes

| | |
|---|---|
| Pull request | [#32](https://github.com/Aegis2D-Dev/project-rtype/pull/32) (opened 2026-10-16) |
| Branch | `feat/server-tcp-disconnect` → `develop` |
| Status | Merged on 2026-10-16 (`c962c4a`) |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#116 detect disconnect on TCP close |

## Summary
A client that crashes or is killed never sends `Logout`, but the system closes its TCP
connection, and the server sees it at once: the session ends, the player leaves the game,
its ship is removed at the next tick, and the other players get its `Destroy`. This
already worked through `ServerNetwork` and the events of #31; the server's wiring now
lives in a class, `GameServer`, so that a test can run the whole server and check the
ticket's criterion end to end. Part of the game server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `rtype/game_server.hpp`: `GameServer`, the whole server, `ServerNetwork` on the
  caller's context and the game on its `Simulation` thread, wired as `r-type_server` was:
  bound players join, ended sessions leave, inputs steer, and after each tick the events
  and every third tick a snapshot go to the players.
- Test: two clients over real sockets, one closes its TCP connection; the other gets the
  `Destroy` (`PlayerLeft`) of its ship within a second, the session is gone, the UDP
  endpoint is unbound, and a new client gets the player number back with a new ship.

## Changed
- `r-type_server` runs a `GameServer` instead of wiring the pieces itself.
- Architecture page (`GameServer`, how a player leaves), *Getting started*.
- Changelog: #31 (`b7565e5`) marked as merged, with its commits.

## Commits
- `81cc6b3` feat(server): removed players whose tcp connection closes
- `b1612e7` feat(docs): documented tcp disconnects and updated changelogs
- `4c30222` feat(changelog): updated changelogs

## Notes
- A connection that dies without closing (a cable pulled, a machine frozen) sends nothing
  the server can see: the UDP heartbeat timeout of #117 covers it.
- Verified on Linux (NixOS, Clang 20): a client logged in over TCP and bound over UDP
  (socat), killed with `kill -9`: `r-type_server` logged `TCP: player 0 disconnected` 1 ms
  later. 98 tests, also under ASan + UBSan and ThreadSanitizer, the new one 20 times in a
  row (5 under ThreadSanitizer); clang-format and clang-tidy 22 on the touched files.
