# UDP endpoints bound to sessions

| | |
|---|---|
| Pull request | [#26](https://github.com/Aegis2D-Dev/project-rtype/pull/26) (opened 2026-10-15) |
| Branch | `feat/server-udp-binding` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#110 bind UDP endpoint to session |

## Summary
A logged-in client binds its UDP endpoint to its session with a `Connect` datagram,
answered by `Accept` (RFC section 3.4). Every other datagram must come from a bound
endpoint with the token of its session; the others are dropped without an answer, in the
order of RFC section 6.3. The game loop (#112) reads the datagrams of bound players. Part
of the game server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `SessionTable::bind()` and `BindResult`: a `Connect` binds its source endpoint to the
  session of its token (`Bound`), is answered again from the same endpoint
  (`AlreadyBound`), and is refused for an unknown token (`UnknownSession`) or a session or
  endpoint bound already (`Taken`), so a third party who learned a token cannot take the
  session over. `SessionTable::find(endpoint)`; `Session::endpoint`.
- `ServerNetwork::dropped_datagrams()` and `game_datagrams()`.
- Tests: `SessionTable::bind()` alone, and over real UDP sockets the `Accept` and its
  acknowledgements, 8 kinds of dropped datagrams, and the unbinding after a `Logout`.

## Changed
- `ServerNetwork`: each datagram is checked for its header, direction and source (RFC
  section 6.3, steps 2 to 4). Dropped datagrams are counted and logged at the debug level,
  never answered. Each bound session has a `net::ReliableChannel`, so `Accept` carries a
  sequence and acknowledges the `Connect`. Ending a session unbinds its endpoint.
- *Getting started*: what the server does now.
- Changelog: #25 (`d7ee3f6`) marked as merged, with its commits.

## Commits
- `b570d18` feat(server): bound udp endpoints to sessions

## Notes
- The payloads of game datagrams (step 5 of section 6.3) and their acknowledgements are
  left to the game loop (#112 to #115); the per-source drop summary and the rate limit to
  #120.
- Verified: 68 tests, the server ones 20 times in a row, the suite under ASan and UBSan,
  clang-format and clang-tidy 22, Doxygen, `mkdocs build --strict`.
