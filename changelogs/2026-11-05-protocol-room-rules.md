# Room rules

| | |
|---|---|
| Pull request | [#80](https://github.com/Aegis2D-Dev/project-rtype/pull/80) (opened 2026-11-05) |
| Branch | `feat/protocol-room-rules` → `develop` (stacked on `feat/client-room-screen`, #79) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#306](https://github.com/Aegis2D-Dev/aegis2d/issues/306) feat(protocol): room rules |

## Summary
A room is created with the rules of its game: lives, difficulty, waves file and friendly fire,
as section 3.6 of the RFC now says (Aegis2D-Dev/aegis2d#456). The server checks them in order
and refuses the room with the reason of the first bad one; a valid room keeps its rules for its
games, which apply them with Aegis2D-Dev/aegis2d#307. Eighth ticket of the *Network:
multi-instance server* epic (Aegis2D-Dev/aegis2d#298).

## Added
- `RoomRules` (`lives`, `difficulty`, `waves`, `friendly_fire`, as received), `Difficulty`
  (`Easy`, `Normal`, `Hard`), `max_room_lives` (9) and `CreateRoomRequest::rules`, read and
  written with `CreateRoom`; `RoomRejection` `BadLives`, `BadDifficulty`, `UnknownWaves` and
  `BadFriendlyFire`.
- `SessionTable::set_waves()` and `rules()`; `ServerNetwork::set_waves()`; `waves_names()`,
  the `.json` files of a folder. `r-type_server` offers the waves files next to its own.
- Tests: the bytes of the RFC examples; the rules read as received; each bad rule refused with
  its reason, in the order of the RFC; valid rules kept, and the default ones for a room opened
  for a login; a refused room over TCP, the connection going on; the waves files of a folder.

## Changed
- `SessionTable::create_room()` checks the rules after the name and the size.
- `ClientSession`: a sentence for each new reason of `RoomRejected`.
- Tests: the `CreateRoom` messages carry the rules.
- *Architecture*: the rules of a room.

## Commits
- `3950e98` feat(protocol): added the rules of a room, checked by the server
- `65e2be9` feat(tests): added room rules refused with their reason
- feat(docs): documented the room rules and updated changelogs

## Notes
- Stacked on #79 (itself on #78 to #74), as it changes the files of the room messages they
  changed: merge them first. The RFC part is Aegis2D-Dev/aegis2d#456.
- The rooms screen creates rooms with the default rules: choosing them on the screen is not
  part of the ticket.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 (71 files, and 80 with the client),
  `mkdocs build --strict`, Doxygen.
- `ctest`: 256 of 257 tests pass; under ASan and UBSan, all the test cases of the server,
  client, bot and waves but one. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
