# Room screen

| | |
|---|---|
| Pull request | [#79](https://github.com/Aegis2D-Dev/project-rtype/pull/79) (opened 2026-11-05) |
| Branch | `feat/client-room-screen` → `develop` (stacked on `feat/client-room-list`, #78) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#305](https://github.com/Aegis2D-Dev/aegis2d/issues/305) feat(client): room screen |

## Summary
A room joined from the rooms screen now has a screen of its own: its players, each ready or
not, the player's own highlighted; Enter says the player is ready (or not any more), Escape
goes back to the rooms, and the game starts for everyone once every player of the room is
ready. Four clients fill and start a room while a second room plays. Seventh ticket of the
*Network: multi-instance server* epic (Aegis2D-Dev/aegis2d#298), on the rooms screen of #78.

## Added
- `ClientState::InRoom`: after `RoomJoined`, the session follows the players of its room
  (`room_players()`, from `RoomPlayers`), `set_ready()` and `leave_room()` send their
  requests, `ready()` says what the player said, and `GameStart` starts the binding;
  `leave_room()` goes back to the lobby, the rooms listed again at once.
- `r-type_client`: the room screen (`ClientScreen::Room`, `draw_room()`): the room's name,
  one row per player with its number, name and state, how many are ready, the last message
  and the keys.
- Tests: four clients that choose their room create one and join it, one leaves and comes
  back, three get ready while all see it and nothing starts, then the fourth: the four play
  in that room, numbered 0 to 3, while a player of the first room, who did not choose its
  room, plays on there.

## Changed
- `ClientSession`: a `RoomJoined` no longer starts the binding at once; the `RoomPlayers` of
  the room given at login, before a room is joined, are ignored.
- `r-type_client`: the rooms screen leads to the room screen.
- Tests: the client of the rooms screen test gets ready in its room before it plays.
- *Play*: the room screen. *Architecture*: the `InRoom` state.
- Changelog, with `develop` merged in: #78 (`1dba810`) marked as merged.

## Commits
- `693421f` feat(client): added the room screen, ready to start or leave
- `d7a797c` feat(tests): added four clients starting a room while another plays
- `f6eb65a` feat(docs): documented the room screen and updated changelogs
- Merge remote-tracking branch 'origin/develop' into feat/client-room-screen

## Notes
- Stacked on #78 (itself on #77, #76, #75 and #74), all merged on 2026-11-05; retargeted to
  `develop`, which was merged in: the only conflict was the index of `CHANGELOG.md`.
- The screen was built (client with `SDL_UNIX_CONSOLE_BUILD`, Clang 20, warnings as errors)
  and checked by clang-tidy, but not seen in a window: the machine used has no display.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors):
  clang-format, clang-tidy 22 (71 files, and 80 with the client), `mkdocs build --strict`,
  Doxygen.
- `ctest`: 254 of 255 tests pass; under ASan and UBSan, 205 of 206 test cases of the server,
  client and bot. *a flooding address is cut off, and the players play on* fails on the
  machine used, as on `develop`, and passes in CI.
