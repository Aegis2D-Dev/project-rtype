# Kick and ban from the admin console, the bans kept across restarts

| | |
|---|---|
| Pull request | [#100](https://github.com/Aegis2D-Dev/project-rtype/pull/100) (opened 2026-11-07) |
| Branch | `feat/server-kick-ban` → `develop` (stacked on `feat/server-admin-console`, #99) |
| Status | Merged on 2026-11-07 (`d41d415`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#311](https://github.com/Aegis2D-Dev/aegis2d/issues/311) feat(server): kick and ban |

## Summary
The admin console of #99 gets `kick <player>` and `ban <player>`. The player is told why
(`Logout` with `Kicked` or `Banned`), the other players get a message of the server saying
so, and its game a `PlayerLeft` with `Kicked`. A ban is kept in a JSON file, `bans.json` by
default, written at once and read back at start: a banned address cannot log in again, even
after a restart, and gets `LoginRejected` with `Banned`. The protocol part is
Aegis2D-Dev/aegis2d#482. Fifteenth ticket of the *Network: multi-instance server* epic
(Aegis2D-Dev/aegis2d#298).

## Added
- `BanList`: banned IP addresses, port ignored, in a JSON file written through a temporary
  file; a missing file is an empty list, a corrupted one is reported and never fatal.
- `ServerNetwork::remove_player()`, `set_bans()` and `bans()`; a banned address is refused
  before anything else of its login.
- The `kick` and `ban` commands; `ban` says the address banned, and refuses when the server
  keeps no bans.
- `LoginRejection::Banned` (6), `LogoutReason::Kicked` (5) and `Banned` (6).
- `--bans <file.json>` on `r-type_server`, `ServerOptions::bans` and `default_bans_file`.
- Tests: the bans read back by a new list whatever the port, IPv6 included; four corrupted
  files reported, the next ban writing a valid file; a kicked player told why and the other
  told it was removed; a banned player told why, its address refused at the next login, and
  again by a new server reading the file; an unknown player, a server without bans; the
  option.

## Changed
- `ClientSession`: sentences for `Kicked`, `Banned` and a login refused as `Banned`.
- `r-type_server` prints its bans file and how many addresses it bans.
- `.gitignore`: `bans.json` of a server run from the clone.
- Tests: the unknown command of the console test is no longer `kick`.
- *Getting started*: `kick`, `ban` and `--bans`. *Architecture*: the bans.

## Commits
- `d30e71b` feat(server): added kick and ban, the bans kept on disk and checked at login
- `71f8ddc` feat(tests): added kicked and banned players, and bans kept across a restart
- feat(docs): documented kick and ban and updated changelogs

## Notes
- Stacked on #99, the admin console it adds commands to.
- A ban covers the address, not the name: other sessions from that address stay until they
  log out, and none can log in again. Lifting a ban is done by hand in the file: the ticket
  asks for no `unban`.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 (86 files, and 98 with the client),
  `mkdocs build --strict`, Doxygen.
- `ctest`: 314 of 315 tests pass; under ASan and UBSan, the test cases of the console, the
  bans, the logins and the chat pass. *a flooding address is cut off, and the players play
  on* fails on the machine used, as on `develop`, and passes in CI.
