# Errors of one client contained

| | |
|---|---|
| Pull request | [#35](https://github.com/Aegis2D-Dev/project-rtype/pull/35) (opened 2026-10-16) |
| Branch | `fix/server-client-errors` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#119 isolate per-client errors |

## Summary
The engine's sockets go on receiving after a handler throws, but they pass the exception
on: it left `context.run_for()`, and `r-type_server`, which only caught `SocketError`, would
stop for every player because of one packet. The handling of each datagram and TCP message
is now contained: the exception is logged, the client that sent the packet is kicked as
RFC section 6.6 says (`Logout` with `ProtocolError`, `PlayerLeft` with `Kicked` for the
others), and the server goes on. Part of the game server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `ServerNetwork`: every accepted connection, datagram, TCP message and closed connection is
  handled inside a guard that logs an exception instead of letting it out; the sender of a
  datagram or message that threw is kicked (a connection not logged in yet is closed);
  `errors()` counts them.
- `r-type_server`: its loop logs anything else that throws (a task posted by the game) and
  goes on.
- Tests: exceptions injected in the game's input handler and in its leave handler, the
  culprit kicked with `ProtocolError` and the other player still playing; and 10,000
  forged datagrams (noise, valid headers with any payload, valid datagrams with bytes
  changed) plus TCP garbage from three logged-in, bound attackers against a `GameServer`:
  the honest player keeps its session and its snapshots, and nothing throws.

## Changed
- `ServerNetwork` forgets a session before calling the leave handler, so a handler that
  throws cannot leave the session behind.
- Architecture page and *Getting started*: a client can only break its own session.
- Changelog: #34 (`070bdf1`) marked as merged, with its commits.

## Commits
- `483b2e8` feat(server): contained the errors of each client
- `48a793d` feat(docs): documented error isolation and updated changelogs
- feat(changelog): updated changelogs

## Notes
- The parsing itself never throws on bad input (checked reads, fuzzed in the engine, #104
  and #105); the guards are for bugs and for errors such as running out of memory.
- Verified on Linux (NixOS, Clang 20) against the engine's `develop`: `r-type_server` built
  with ASan + UBSan, flooded for 8 s with 1,761 forged packets (random UDP datagrams, the
  protocol's magic followed by noise, random TCP messages): still running at 60 Hz, no
  error and no sanitizer report. 105 tests, also under ASan + UBSan and ThreadSanitizer,
  the new ones 20 times in a row (5 under each sanitizer); clang-format and clang-tidy 22
  on the touched files.
