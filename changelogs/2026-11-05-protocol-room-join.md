# Join, leave and start a room

| | |
|---|---|
| Pull request | [#77](https://github.com/Aegis2D-Dev/project-rtype/pull/77) (opened 2026-11-05) |
| Branch | `feat/protocol-room-join` → `develop` (stacked on `feat/protocol-room-list`, #76) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#303](https://github.com/Aegis2D-Dev/aegis2d/issues/303) feat(protocol): join, leave and start a room |

## Summary
Out of game, a client joins a room (`JoinRoom`, answered by `RoomJoined` or `JoinRejected`),
leaves it for the lobby (`LeaveRoom`) and says when it is ready (`SetReady`); the server sends
it the players of its room and their ready states (`RoomPlayers`) whenever they change, then
`GameStart` once every player of the room out of game is ready, as section 3.7 of the RFC now
says (Aegis2D-Dev/aegis2d#455). Four clients join one room and start its game with these
messages. A client that never uses the rooms, as the game's client and bot for now, binds
right after its login and plays as before. Fifth ticket of the *Network: multi-instance
server* epic (Aegis2D-Dev/aegis2d#298), on the room messages of #76.

## Added
- `rtype/rooms.hpp`: `JoinRoom`, `RoomJoined`, `JoinRejected`, `LeaveRoom`, `SetReady`,
  `RoomPlayers` and `GameStart`, their readers and writers, `JoinRejection`, `RoomPlayer` and
  `RoomPlayers`; `TcpKind` 10 to 16.
- `SessionTable::join()`, `leave()`, `set_ready()` and `members()`; `Session::ready`;
  `no_room`, the room of a session in the lobby; `BindResult::NoRoom`, a `Connect` from the
  lobby, dropped.
- `ServerNetwork`: the requests of the rooms answered after the login; `RoomPlayers` of a room
  to its clients out of game that used the rooms, after a login, a join, a leave, a change of
  ready state, a binding or the end of a session, then `GameStart` once they are all ready.
- Tests: the bytes of the RFC example; each message read whole or not at all; sessions out of
  game joining and leaving rooms (numbers, full rooms, rooms not in use, own room, the lobby,
  ready states), and staying in their room once in game; four clients create and join a room,
  get ready one after the other, get `GameStart` once the last one is, then bind and play in
  that room, while a fifth is refused the full room and a room not in use, then leaves for the
  lobby, whose `Connect` finds no game.

## Changed
- `SessionTable` keeps one list of sessions instead of 4 slots per room: a session may be in
  the lobby, and moves between rooms. The server still takes 4 sessions per room, the lobby
  included; `login()` and the other functions behave as before.
- Tests: kind `16` is `GameStart`, the first reserved kind is `17`.
- *Architecture* and *Getting started*: joining, leaving and starting a room.

## Commits
- `b240be7` feat(protocol): added joining, leaving and starting a room
- `52f4e7f` feat(tests): added four clients joining a room and starting its game
- feat(docs): documented joining and starting a room and updated changelogs

## Notes
- Stacked on #76 (itself on #75 and #74): merge them first. The RFC part is
  Aegis2D-Dev/aegis2d#455.
- 582 lines added and 110 removed in `protocol/` and `server/`, documentation comments
  included, past the 400 of the ticket: the session table moved to one list of sessions, which
  rewrites most of it, and each of the 7 messages has its reader, writer and documentation.
- The room screens of the client come with Aegis2D-Dev/aegis2d#304 and #305; until then, the
  client and the bot never send these messages, so they get none of the new ones.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors):
  clang-format, clang-tidy 22 (70 files), `mkdocs build --strict`, Doxygen.
- `ctest`: 251 of 252 tests pass; under ASan and UBSan, 202 of 203 test cases of the server,
  client and bot. *a flooding address is cut off, and the players play on* fails on the
  machine used, as on `develop`, and passes in CI.
