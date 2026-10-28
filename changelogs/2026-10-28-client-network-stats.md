# Network statistics

| | |
|---|---|
| Pull request | [#51](https://github.com/Aegis2D-Dev/project-rtype/pull/51) (opened 2026-10-28) |
| Branch | `feat/client-network-stats` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#271](https://github.com/Aegis2D-Dev/aegis2d/issues/271) network statistics |

## Summary
The client measures what it receives from the server over the last 2 seconds: the
datagrams lost, from the gaps in the server's sequences, the mean time between two
snapshots and their jitter. With the traffic counters (#48) and the round trip time (#50),
they make the network statistics the lagometer will show. Third ticket of the *Network:
measurements and lag tools* epic (Aegis2D-Dev/aegis2d#268).

## Added
- `rtype/network_stats.hpp`: `NetworkStats`, fed with every datagram the client receives:
  `record(sequence, type, arrival)`, then `summary(now)` over the last `window` (2 s):
  `NetworkSummary` with the datagrams received and lost, the loss (0 to 1), the snapshots,
  their mean interval and their jitter (mean distance of each interval from the mean).
- `ClientSession::network_stats()`: the statistics of the session, recorded since the login.
- Tests: a regular stream, one datagram in four dropped, every other snapshot 10 ms late,
  both at once, duplicates and late datagrams, sequences across their wrap, the window
  forgetting old datagrams; and a real client fed by a server that skips 3 of 20
  snapshots.
- *Architecture*: what the session measures, and `network_stats` in the code map.

## Changed
- Changelog: #50 merged on 2026-10-27 (`77cc529`), with its last commit.

## Commits
- `4c7abeb` feat(client): measured the loss and jitter of the server's datagrams
- `74d35ab` feat(tests): added loss and jitter tests with dropped and late datagrams
- feat(docs): documented the network statistics and updated changelogs

## Notes
- A datagram is lost when its sequence is missing between the oldest and the newest of the
  window: the very last ones, not followed by any yet, are not counted until one is.
- Events and snapshots share the server's sequence, so the loss counts both; only the
  snapshots, sent at a fixed rate, give the interval and the jitter. A lost snapshot shows
  as an interval twice as long.
- Verified: 156 tests (`server` configuration against the engine's `develop`, the new ones
  5 times in a row), warnings as errors, clang-format and clang-tidy 22.
  `mkdocs build --strict` not run locally (MkDocs not installed); the *Documentation*
  workflow builds it.
