# List and create rooms

| | |
|---|---|
| Pull request | [#76](https://github.com/Aegis2D-Dev/project-rtype/pull/76) (opened 2026-11-05) |
| Branch | `feat/protocol-room-list` → `develop` (stacked on `feat/server-room-lifecycle`, #75) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#302](https://github.com/Aegis2D-Dev/aegis2d/issues/302) feat(protocol): list and create rooms |

## Summary
A logged-in client lists the rooms over TCP, with their number, name, players, size and
state, and creates one, empty, for the next logins, as section 3.6 of the RFC now says
(Aegis2D-Dev/aegis2d#454). The server keeps the rooms in use with their name and size: a
login goes to the first one with a free place, or else opens `Room <n>` for 4 players. Fourth
ticket of the *Network: multi-instance server* epic (Aegis2D-Dev/aegis2d#298), on the room
lifecycle of #75.

## Added
- `rtype/rooms.hpp`, in `rtype_protocol`: `ListRooms`, `RoomList`, `CreateRoom`,
  `RoomCreated` and `RoomRejected`, their readers and writers, `RoomRecord`, `RoomState`,
  `RoomRejection`, `CreateRoomRequest` and `max_listed_rooms` (64).
- `TcpKind::ListRooms` to `TcpKind::RoomRejected` (5 to 9).
- `SessionTable::create_room()` and `list_rooms()`: a room is in use from its creation, by
  `CreateRoom` or for a login that found no free place, until its last session ends; its
  state is `Playing` once one of its sessions is bound.
- `ServerNetwork`: after the login, a `ListRooms` gets the `RoomList` and a `CreateRoom` its
  `RoomCreated` or `RoomRejected`, logged; a malformed one ends the session as any other bad
  message.
- Tests: the bytes of the RFC example; each message read whole or not at all; rooms created
  (names, sizes, none left), listed, filled by the logins up to their size, playing once a
  player is bound and ended with their last player; a client lists and creates rooms over TCP,
  and a malformed `ListRooms` ends its session without an answer.

## Changed
- `SessionTable::login()`: a session goes to the first room in use with a free place, below its
  size, instead of the first room with a free number; `set_rooms()` takes at most 64 rooms.
- `LoginRejection::ServerFull` and `NameTaken`, and `LoginAccepted::player`: documented for a
  server of several rooms.
- Tests: kind `5` is `ListRooms`, the first reserved kind is `10`.
- *Architecture*: the room messages. *Getting started*: rooms, and each game ticking while it
  has players (since #75).

## Commits
- `6d4b611` feat(protocol): added the messages to list and create rooms
- `dacb191` feat(tests): added rooms listed and created over tcp
- feat(docs): documented the room messages and updated changelogs

## Notes
- Stacked on #75 (itself on #74): merge them first. The RFC part is Aegis2D-Dev/aegis2d#454.
- Joining and leaving a given room come with Aegis2D-Dev/aegis2d#303; until then, a created
  room is filled by the next logins.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors):
  clang-format, clang-tidy 22 (70 files), `mkdocs build --strict`, Doxygen.
- `ctest`: 247 of 248 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
