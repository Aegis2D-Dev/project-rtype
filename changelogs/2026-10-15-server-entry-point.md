# Server entry point with port arguments

| | |
|---|---|
| Pull request | [#24](https://github.com/Aegis2D-Dev/project-rtype/pull/24) (opened 2026-10-15) |
| Branch | `feat/server-entry-point` → `develop` |
| Status | Merged on 2026-10-15 (`1c6d1d7`) |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#108 entry point with port arguments |

## Summary
`r-type_server` now listens on a TCP port for the logins and a UDP port for the game, given
with `--tcp-port` and `--udp-port` (4242 and 4243 by default), as section 1.3 of the
protocol RFC requires. It is the base of the server tickets that follow: the login (#109),
the binding of UDP endpoints (#110) and the game loop (#112) plug into it. Part of the game
server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `rtype::parse_server_options()` (`server/include/rtype/server_options.hpp`): reads
  `--tcp-port`, `--udp-port`, `--waves`, `--help` and `--version` into `ServerOptions`, or
  a message naming the faulty argument. Ports are decimal numbers from 0 to 65535, 0 asking
  the system for a free port.
- `rtype::ServerNetwork` (`server/include/rtype/server_network.hpp`): binds both ports with
  the engine's `TcpListener` and `UdpSocket`, so a port in use stops the server at start.
  Until the login exists, it closes each TCP connection (info log) and counts and drops
  datagrams (debug log).
- Static library `rtype_server`, shared by `r-type_server` and the tests.
- `--help`, which prints the usage.
- Tests (`tests/server_test.cpp`): the command line (defaults, order, repeats, 8 malformed
  ports, unknown options, missing values), a TCP connection and a datagram received on the
  ports asked for, and ports already in use.
- After the first CI run: `parse_server_options()` returns a `std::variant` instead of a
  `std::expected`, which the standard library of the CI image (Ubuntu 24.04) lacks, and
  reads its arguments without `std::span::at()` (C++26), which Windows lacks too.

## Changed
- `r-type_server` loads the waves, prints `listening: TCP port …, UDP port …` and runs until
  stopped (Ctrl+C) instead of exiting. A bad command line still exits with status 2; a port
  in use exits with status 1, like a bad waves file.
- *Code quality* workflow, Linux job: the server runs on free ports under `timeout 2`, and
  the step checks that it was still running when stopped.
- README and *Getting started*: the server options, their defaults and the exit statuses.
- Doxygen reference: the server headers are included.
- Changelog: #23 (`8af5daf`) marked as merged, with its commits.

## Commits
- `915bd2a` feat(server): added tcp and udp port arguments
- `aafd78a` feat(changelog): updated changelogs
- `82b0480` feat(server): replaced std::expected for older standard libraries

## Notes
- The clean shutdown on SIGINT and SIGTERM is Aegis2D-Dev/aegis2d#121: for now Ctrl+C ends
  the process at once.
- Verified: server preset with clang 20 and warnings as errors, 52 tests, clang-format and
  clang-tidy 22, Doxygen, `mkdocs build --strict`.
