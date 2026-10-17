# Clean stop on SIGINT and SIGTERM

| | |
|---|---|
| Pull request | [#36](https://github.com/Aegis2D-Dev/project-rtype/pull/36) (opened 2026-10-17) |
| Branch | `feat/server-graceful-shutdown` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#121 graceful shutdown on SIGINT and SIGTERM |

## Summary
Ctrl+C (SIGINT) and SIGTERM stop the server cleanly instead of killing it: every player
gets `Logout` with the reason `Shutdown` (RFC section 3.2), the connections close once it
has left, the simulation thread stops and the server exits with status 0. A second signal
quits at once. Part of the game server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `rtype/stop_signal.hpp`: `install_stop_handlers()` and `stop_requested()`. The first
  signal only sets a lock-free atomic flag; a second one ends the process with status 130.
- `ServerNetwork::shutdown()`: no more connections accepted, `Logout(Shutdown)` to every
  logged-in player, the other connections closed; `idle()` once none is left.
- Tests: the shutdown over real sockets, and `SIGTERM` setting the flag.

## Changed
- `r-type_server` runs until a stop is asked, then lets the `Logout` messages leave for 1
  second at most before stopping the simulation and closing the sockets; it exits with
  status 0. Exceptions other than socket errors no longer escape `main`.
- *Code quality* workflow, Linux job: the server is stopped with SIGINT after 2 seconds and
  must exit with status 0 within 5 more seconds.
- *Getting started*: how to stop the server.
- Changelog: #35 (`0d4f038`) marked as merged, with its commits.

## Commits
- `f17b3c2` feat(server): stopped cleanly on sigint and sigterm

## Notes
- Verified: 107 tests, under ASan and UBSan, the server tests and a SIGINT stop under
  ThreadSanitizer, clang-format and clang-tidy 22, Doxygen, `mkdocs build --strict`, and
  SIGINT to `r-type_server` with two clients connected.
