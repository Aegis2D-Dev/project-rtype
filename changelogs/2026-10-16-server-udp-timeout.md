# Disconnect detected when UDP goes silent

| | |
|---|---|
| Pull request | [#33](https://github.com/Aegis2D-Dev/project-rtype/pull/33) (opened 2026-10-16) |
| Branch | `feat/server-udp-timeout` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#117 detect disconnect on UDP heartbeat timeout |

## Summary
A player whose UDP is blocked, or whose machine froze, keeps its TCP connection open and
sends nothing the server could see as a disconnection. RFC section 5.5 says what to do: a
bound player that sends no valid datagram for 5 seconds has lost its session. The server
now tells it so with `Logout` (`TimedOut`), closes its connection once the message is
sent, and the player leaves the game: its ship is removed and the others get its
`Destroy`. Part of the game server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `ServerNetwork::udp_timeout` (5 s): the time a bound player may send no valid datagram.
  Every valid datagram of the player resets it: `Connect`, `Input`, `Heartbeat`, `Event`;
  a malformed or foreign one does not.
- `ServerNetwork::update()` ends the sessions silent for that long: `Logout` with the reason
  `TimedOut`, the connection closed once it is sent, an info log, the player leaving the
  game; `timeouts()` counts them.
- Tests: two bound players, one sending a `Heartbeat` and the other only a malformed
  datagram; past the timeout, only the silent one is lost, gets `Logout` (`TimedOut`) then
  its connection closes, and its endpoint is unbound. End to end with `GameServer`: a
  player whose datagrams stop is gone from the game, the other getting the `Destroy` of its
  ship.

## Changed
- Architecture page and *Getting started*: a player silent for 5 seconds leaves the game.
- Changelog: #32 (`c962c4a`) marked as merged, with its commits.

## Commits
- `1328931` feat(server): ended sessions silent on udp for 5 seconds
- `013cf5d` feat(docs): documented the udp timeout and updated changelogs
- feat(changelog): updated changelogs

## Notes
- Only bound players time out. A logged-in client that never binds its endpoint keeps its
  session while its TCP connection is open, as before (the tests rely on it); the RFC has
  the client give up after 5 seconds (section 3.4), but a client that does not still holds
  a player slot.
- Verified on Linux (NixOS, Clang 20) against the engine's `develop`: in a user network
  namespace, a client logged in over TCP, bound over UDP and sending a `Heartbeat` every 50
  ms; its UDP blocked with the engine's `tools/netem.sh --ports 50000 --loss 100%
  --duplicate 0%`: `r-type_server` ended the session 5.00 s later, twice, and the client
  read `Logout` (`TimedOut`) on its TCP connection. 100 tests, also under ASan + UBSan and
  ThreadSanitizer, the new ones 20 times in a row (5 under each sanitizer); clang-format
  and clang-tidy 22 on the touched files.
