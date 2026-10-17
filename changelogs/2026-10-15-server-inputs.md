# Client inputs applied to player ships

| | |
|---|---|
| Pull request | [#29](https://github.com/Aegis2D-Dev/project-rtype/pull/29) (opened 2026-10-15) |
| Branch | `feat/server-inputs` → `develop` |
| Status | Merged on 2026-10-15 (`0da9f65`) |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#113 apply client inputs to player entities |

## Summary
The inputs of the players steer their ships. The server checks the payload of every game
datagram of a bound player, acknowledges the valid ones, and passes the most recent
`Input` of each player to the game on the simulation thread. `Disconnect` ends a session
like `Logout`. Part of the game server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `ServerNetwork::on_input()`: called with the session and the held actions of each `Input`
  more recent than the last one applied.
- `GameInstance::set_input()`: the actions of a player, applied to its ship at every tick
  until the next `Input`.
- Tests: valid, malformed, late and duplicated inputs over real sockets, with the
  acknowledgements they get; heartbeats, events and disconnects; an input moving a ship.

## Changed
- `ServerNetwork`: datagrams of bound sessions are read whole and checked (RFC sections
  4.2, 5.5, 6.3, 6.4): `Input` is one byte with bits 5 to 7 clear, `Heartbeat` and
  `Disconnect` are empty, `Event` is well framed (its events ignored, as only the server
  sends events). Valid ones are recorded in the session's `ReliableChannel` and so
  acknowledged; malformed ones are dropped and not acknowledged. A `Disconnect` ends the
  session and closes its TCP connection.
- `r-type_server` posts each input to the simulation thread.
- *Getting started*: what the server does now.
- Changelog: #28 (`5220b4a`) marked as merged, with its commits.

## Commits
- `7cef0a0` feat(server): applied client inputs to player ships
- `959c64e` feat(changelog): updated changelogs

## Notes
- The acknowledgements reach the player in the server's next datagram: `Accept` for now,
  the snapshots of #114 next.
- Verified: 80 tests, the network ones 20 times in a row, the suite under ASan and UBSan,
  the server tests under ThreadSanitizer, clang-format and clang-tidy 22, Doxygen,
  `mkdocs build --strict`.
