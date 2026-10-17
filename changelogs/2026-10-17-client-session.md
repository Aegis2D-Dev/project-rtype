# Client login and UDP binding

| | |
|---|---|
| Pull request | [#38](https://github.com/Aegis2D-Dev/project-rtype/pull/38) (opened 2026-10-17) |
| Branch | `feat/client-session` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#124 TCP login then UDP handshake |

## Summary
The client joins a server: it logs in over TCP, receives its token, binds its UDP endpoint
and reaches the game state (RFC section 3), or tells the player why it cannot. The TCP
messages move to a library shared by the server and the client. First ticket of the client
networking epic (Aegis2D-Dev/aegis2d#122).

## Added
- `protocol/` (`rtype_protocol`): the TCP messages of the session, shared by the server and
  the client; `read_login_accepted()` and `read_login_rejected()`.
- `client/network/` (`rtype_client_net`, built without SDL):
  - `ClientSession`: TCP login, then `Connect` every 250 ms until `Accept`; failures with a
    sentence for the player (refused login, server absent or silent for 5 seconds, UDP
    port blocked, connection closed); only the server's datagrams with the session's
    token are read; `leave()` sends `Disconnect` three times and `Logout`.
  - `parse_client_options()`: `--host`, `--port`, `--name`, `--help`, `--version`.
- Tests: the session against a real `ServerNetwork` and a fake server silent over UDP, the
  failures, the options, and the readers of the login answers.

## Changed
- `server/login.hpp` and `server/login.cpp` moved to `protocol/`; the include path is the
  same.
- `r-type_client` joins the server, prints its player number and leaves; status 2 for a
  bad command line, 1 when it cannot join.
- *Getting started*: the client's options; README: the repository layout; Doxygen: the new
  headers.
- Changelog: #37 (`d599aff`) marked as merged, with its commits.

## Commits
- `2dc2a80` feat(client): logged in over tcp and bound the udp endpoint

## Notes
- `r-type_client` itself needs SDL3 and OpenGL: its `main` was compiled and linted against
  the real engine headers, run with stubs of the render and audio module names against
  `r-type_server`, and is built by the CI jobs.
- Verified: 122 tests, the client ones 10 times in a row, under ASan and UBSan and
  ThreadSanitizer, clang-format and clang-tidy 22, Doxygen, `mkdocs build --strict`.
