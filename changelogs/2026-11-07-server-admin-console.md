# Admin console on the server's standard input

| | |
|---|---|
| Pull request | [#99](https://github.com/Aegis2D-Dev/project-rtype/pull/99) (opened 2026-11-07) |
| Branch | `feat/server-admin-console` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#310](https://github.com/Aegis2D-Dev/aegis2d/issues/310) feat(server): admin console |

## Summary
The administrator of `r-type_server` types commands on its standard input, through the
engine's command system: `rooms` and `players` list the rooms and the players, `say` sends a
message of the server to every player, `close` ends the sessions of a room, each player told
why, and `stop` stops the server as Ctrl+C does. A wrong command, a missing argument or one
that cannot be done says why and changes nothing. The protocol part is
Aegis2D-Dev/aegis2d#481. Fourteenth ticket of the *Network: multi-instance server* epic
(Aegis2D-Dev/aegis2d#298).

## Added
- `AdminConsole`: `rooms`, `players`, `say <text>`, `close <room>`, `stop` and the engine's
  `help`, on `core::Commands`; `stop` does what it is given (`request_stop()` in
  `r-type_server`).
- `ConsoleInput`: the lines of a stream, read on a detached thread that shares only them, so
  a server waiting for a line can still stop.
- `ServerNetwork::say()` and `close_room()`; `request_stop()`.
- `LogoutReason::RoomClosed` (4), `server_player` (254) and `server_name` (`"server"`): the
  messages of the server cannot be taken for a player's, whatever the player's name.
- Tests: every command with players in a room and in the lobby, the message of the server
  reaching all of them, the room closed with its players told why and the lobby kept, `stop`;
  a long text, missing and bad arguments, a room not in use and an unknown command refused
  with their reason, nothing changed; the lines of a stream taken until its end.

## Changed
- `r-type_server` reads its standard input while it runs, and says so at start.
- `ClientSession`: a sentence for `RoomClosed`. The chat draws the messages of the server in
  the title's color.
- *Getting started*: the admin console. *Architecture*: its commands.
- Changelog: #97 (`fc246e7`) marked as merged.

## Commits
- `69e9f93` feat(server): added the admin console on the standard input of the server
- `49cd6ca` feat(tests): added every admin command and the wrong ones
- feat(docs): documented the admin console and updated changelogs

## Notes
- Checked by hand too: `r-type_server` with the commands piped on its standard input
  answers each of them, says why for `close 3`, `close`, `say` and an unknown one, and
  `stop` stops it with status 0.
- `kick` and `ban` come with Aegis2D-Dev/aegis2d#311, on these commands.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 (84 files, and 96 with the client),
  `mkdocs build --strict`, Doxygen.
- `ctest`: 311 of 312 tests pass; under ASan and UBSan, the test cases of the console, the
  chat and the logins pass. *a flooding address is cut off, and the players play on* fails on
  the machine used, as on `develop`, and passes in CI.
