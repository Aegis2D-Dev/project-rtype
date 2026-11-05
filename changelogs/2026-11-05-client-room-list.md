# Room list screen

| | |
|---|---|
| Pull request | [#78](https://github.com/Aegis2D-Dev/project-rtype/pull/78) (opened 2026-11-05) |
| Branch | `feat/client-room-list` → `develop` (stacked on `feat/protocol-room-join`, #77) |
| Status | Merged on 2026-11-05 (`1dba810`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#304](https://github.com/Aegis2D-Dev/aegis2d/issues/304) feat(client): room list screen |

## Summary
Once logged in from the connect screen, `r-type_client` shows the rooms of the server,
refreshed every second, with their players and state; the player joins one, or creates one
(name and size) on the last row, then plays in the room joined. A client creates and joins
rooms from the screen, tested against a real server through the screen's model and the
session. `--join` and the bot play at once, as before. Sixth ticket of the *Network:
multi-instance server* epic (Aegis2D-Dev/aegis2d#298), on the room messages of #76 and #77.

## Added
- `ClientSettings::choose_room` and `ClientState::Lobby`: logged in, the session stays in the
  lobby, sends `ListRooms` every `room_refresh` (1 s), and binds once a room is joined, with the
  player's number there; `rooms()`, `create_room()`, `join_room()` and `room_error()`, a
  sentence for each refusal of the server.
- `rtype/room_list_form.hpp`, in `rtype_client_net`: `RoomListForm`, the room list screen
  without its drawing: the rooms, the selected row, kept on its room when the list changes,
  and the new room (name, 1 to 4 players); `submit()` gives the room to join, the room to
  create, or why neither.
- `r-type_client`: the rooms screen (`ClientScreen::Rooms`, `draw_rooms()`), between the
  connect screen and the game: up to 9 rooms around the selected one, the new room, the last
  message, the keys.
- Tests: the form (rows, wrapping, selection kept, name and size of the new room, a full room
  and a nameless room refused); a client in the lobby lists the room of its login, gets the
  reason of a refused room, creates one of 2 players from the form, joins it from the form and
  plays there as its first player, its old room gone.

## Changed
- `ClientSession`: once logged in, it also accepts the `RoomPlayers` and `GameStart` a client
  that used the rooms gets, and says goodbye with `Logout` from the lobby too.
- `r-type_client`: joining from the connect screen goes through the rooms screen.
- *Play*: the rooms screen and its keys. *Architecture*: the lobby of the client.
- Changelog, with `develop` merged in: #77 (`db01923`) marked as merged.

## Commits
- `a589278` feat(client): added the room list screen and the lobby of the session
- `0c30675` feat(tests): added a client creating and joining rooms from the screen
- `d6d39b8` feat(docs): documented the room list screen and updated changelogs
- Merge remote-tracking branch 'origin/develop' into feat/client-room-list

## Notes
- Stacked on #77 (itself on #76, #75 and #74), all merged on 2026-11-05; retargeted to
  `develop`, which was merged in: the only conflict was the index of `CHANGELOG.md`.
- The ready step and the players of the room come with Aegis2D-Dev/aegis2d#305: until then, a
  room joined from the screen is played at once.
- 527 lines added and 35 removed in `client/`, documentation comments included, past the 350
  of the ticket: the lobby of the session, the screen's model and its drawing.
- The screen was built (client with `SDL_UNIX_CONSOLE_BUILD`, Clang 20, warnings as errors)
  and checked by clang-tidy, but not seen in a window: the machine used has no display.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors):
  clang-format, clang-tidy 22 (71 files, and 80 with the client), `mkdocs build --strict`,
  Doxygen.
- `ctest`: 253 of 254 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
