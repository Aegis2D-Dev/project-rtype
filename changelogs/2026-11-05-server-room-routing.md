# Sessions routed to rooms

| | |
|---|---|
| Pull request | [#74](https://github.com/Aegis2D-Dev/project-rtype/pull/74) (opened 2026-11-05) |
| Branch | `feat/server-room-routing` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#300](https://github.com/Aegis2D-Dev/aegis2d/issues/300) feat(server): sessions routed to rooms |

## Summary
Each session now belongs to a room, and its player number is its number in that room. A
`GameServer` runs several rooms side by side, each game on its own thread: the inputs of a
player go to the game of its room only, and the snapshots and events of a room only reach the
players of that room. The logins fill the first room with a free number, then the next one;
`r-type_server` still runs a single room, until rooms are created on demand (#301). Second
ticket of the *Network: multi-instance server* epic (Aegis2D-Dev/aegis2d#298).

## Added
- `Session::room`; `SessionTable::set_rooms()` and `rooms()`: tuning::max_players slots per
  room, a login taking the lowest free number of the first room with one.
- `Room::id()`, given to the constructor.
- `GameServer(..., rate, rooms)`, `GameServer::rooms()` and `room(id)`.
- Tests: logins fill the first room, then the next one, a number freed in the first room
  going first and a name being taken in every room; two rooms of 4 and 2 players, where each
  player gets the ships and names of its room only, the input of player 0 of the second room
  is applied there and not to player 0 of the first, each room's snapshots list its own
  players, and only the other player of the second room learns that one of them left.

## Changed
- `ServerNetwork::send_snapshot()`, `send_events()` and `send_events_to_others()` take the
  room of the game they come from, and send only to the bound players of that room.
- `GameServer`: one room per id, built from the same seed and waves (now taken by reference);
  the players in a game are kept by token with their room, number and rewind, instead of by
  number; joins, leaves, inputs and rewinds go to the room of the session.
- Log: a login and a binding name the room of the player.
- Tests: the 11 calls to `send_snapshot()` and `send_events()` give room 0; the room test
  gives an id.
- *Architecture*: sessions in rooms.
- Changelog: #73 (`e19a22d`) marked as merged.

## Commits
- `0f39824` feat(server): routed each session to a room and each room's traffic to its own
- `c85a261` feat(tests): added two rooms that play side by side without leaking
- `ebed8ca` feat(docs): documented sessions in rooms and updated changelogs
- `7d6fe8f` feat(tests): counted each ship once in the two rooms test
- feat(changelog): updated changelogs

## Notes
- The test of the two rooms fails without the room filter of the events (6 ships seen in a
  room of 4, and the names of the other room), so it does catch a leak. It counts each ship
  once: a player bound while the events of a tick are on their way to the network thread gets
  that tick's `Spawn` of a ship, then the same one in the world sent when it joins, which a
  client applies again harmlessly (RFC section 4.6). It happened once under ASan in CI, on
  #75, where a room's game starts with its first player.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors):
  clang-format, clang-tidy 22 (69 files), `mkdocs build --strict`.
- `ctest`: 240 of 241 tests pass; under ASan and UBSan, 122 of 123 test cases. *a flooding
  address is cut off, and the players play on* fails on the machine used, as on `develop`,
  and passes in CI.
