# TCP login issuing a session token

| | |
|---|---|
| Pull request | [#25](https://github.com/Aegis2D-Dev/project-rtype/pull/25) (opened 2026-10-15) |
| Branch | `feat/server-login` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#109 TCP login handshake issuing a session token |

## Summary
The server runs the TCP login of the protocol RFC (section 3): a client sends
`LoginRequest` and receives a session token, its player number and the UDP port in
`LoginAccepted`, or the reason in `LoginRejected`. Up to 4 players log in; bad, unexpected
or missing logins are closed as section 6.5 says. The binding of UDP endpoints (#110) and
the game instance (#111) build on these sessions. Part of the game server epic
(Aegis2D-Dev/aegis2d#107).

## Added
- `rtype/login.hpp`: the TCP messages of RFC section 3.2 (`LoginRequest`, `LoginAccepted`,
  `LoginRejected`, `Logout`), encoded and decoded with the engine's `BinaryWriter` and
  `BinaryReader`, read whole or rejected; `valid_player_name()` (1 to 16 bytes of valid
  UTF-8, no control characters).
- `rtype::SessionTable` (`rtype/sessions.hpp`): decides each login in the RFC order
  (`BadVersion`, `BadName`, `ServerFull`, `NameTaken`), gives the lowest free player number
  and a random token (never 0, never one in use), and frees the slot and the name when a
  session ends. Tokens come from `core::Random` seeded by `std::random_device`, or from a
  seed in tests.
- Tests: `tests/login_test.cpp` (the RFC example byte by byte, malformed messages, names,
  sessions) and 5 cases of logins over real TCP connections in `tests/server_test.cpp`.

## Changed
- `ServerNetwork`: a valid login gets `LoginAccepted`; a refused one gets `LoginRejected`,
  then the connection closes once the message has left. Empty, malformed or unexpected
  messages, and connections silent for 5 seconds, are closed without a reply. `Logout`, the
  closing of the connection, or any other message after the login ends the session. Logs
  give the address and the player number, never the token. `update()` runs the timeouts.
- `r-type_server` runs the network in 50 ms slices, with `update()` between them.
- *Getting started*: what the server does now.
- Changelog: #24 (`1c6d1d7`) marked as merged, with its commits.

## Commits
- `f038237` feat(server): added the tcp login handshake issuing session tokens

## Notes
- The 5th login already gets `ServerFull`, the "done when" of #111; the game instance
  itself (`ServerGame::add_player`) is left to #111, as is `GameStarted`.
- Verified: 64 tests, the login ones 20 times in a row, the suite under ASan and UBSan,
  clang-format and clang-tidy 22, Doxygen, `mkdocs build --strict`, and a Python client
  logging in to `r-type_server`.
