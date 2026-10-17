# Client connection loss detected

| | |
|---|---|
| Pull request | [#42](https://github.com/Aegis2D-Dev/project-rtype/pull/42) (opened 2026-10-17) |
| Branch | `feat/client-heartbeat` → `develop` |
| Status | Merged on 2026-10-17 (`a2e0dbb`) |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#128 heartbeat and connection-lost screen (network side) |

## Summary
The client knows when a running session ends, and why: the server stops, crashes, sends
nothing for 5 seconds (RFC section 5.5) or ends the session. The connection-lost screen
will show the reason; it needs the client's window, which does not exist yet, so #128
stays open. Part of the client networking epic (Aegis2D-Dev/aegis2d#122).

## Added
- `ClientState::Lost`: a session in game that ended, its reason in `error()`; `Failed` is
  now only a session that never started.
- `ClientSession::server_timeout`: 5 seconds without a valid datagram from the server in
  game loses the session, after a `Logout` over TCP.
- Tests: a server that stops, crashes, goes silent, or keeps sending its snapshots.

## Changed
- `ClientSession`: a `Disconnect` from the server, its `Logout`, or its closed connection
  while in game make the session `Lost`; every valid `Snapshot`, `Event`, `Heartbeat` or
  `Accept` resets the server's silence. In game, the inputs sent every tick meet the 50 ms
  rule of section 5.5, so no separate `Heartbeat` is sent.
- The server-stop test expects `Lost`.
- *Getting started*: the client knows when the connection is lost.
- Changelog: #41 (`d5632dd`) marked as merged, with its commits.

## Commits
- `34b5875` feat(client): detected a lost connection to the server
- `377607e` feat(changelog): updated changelogs

## Notes
- The screen only has to show `session.error()` when the state is `Lost`.
- Verified: 134 tests, the client ones 10 times in a row, under ASan, UBSan and
  ThreadSanitizer, clang-format and clang-tidy 22, Doxygen, `mkdocs build --strict`;
  `client/main.cpp` still compiles.
