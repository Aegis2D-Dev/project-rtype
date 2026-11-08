# Lagometer overlay

| | |
|---|---|
| Pull request | [#52](https://github.com/Aegis2D-Dev/project-rtype/pull/52) (opened 2026-10-29) |
| Branch | `feat/client-lagometer` → `develop` |
| Status | Merged on 2026-10-29 (`3330279`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#272](https://github.com/Aegis2D-Dev/aegis2d/issues/272) lagometer overlay |

## Summary
F6 shows the lagometer over the game: six small graphs of the last 10 seconds (RTT,
snapshot jitter, loss, snapshot interval, bytes in and out), one bar every 100 ms, each
with its last value, green, yellow or red by how far it is from a sound link. It draws
the round trip time (#50), the traffic counters (#48) and the network statistics (#51)
the session already measures. Fourth ticket of the *Network: measurements and lag tools*
epic (Aegis2D-Dev/aegis2d#268).

## Added
- `rtype/lagometer.hpp` (in `rtype_client_net`, so the server preset tests it):
  - `measure_lag(rtt, traffic, stats, now)`: a `LagSample`, one value per `LagGraph` in the
    unit of its graph (ms, %, bytes per second);
  - `lag_level()`: Good, Warning or Bad, from the thresholds of each graph (RTT 100 and
    200 ms, jitter 10 and 30 ms, loss 1 and 5 %, snapshot interval 60 and 100 ms; the
    bandwidth is always Good); `lag_label()`: `"RTT 152 MS"`, `"LOSS 2.5 %"`...;
  - `Lagometer`: `update()` takes a sample every 100 ms (one after a stall, not a burst)
    and keeps the last 100; `scale()` gives the top of a graph, its largest value but
    never below a floor (100 ms for the RTT, 10 % for the loss), so a sound link draws
    low bars.
- `client/lagometer_view`: `draw_lagometer()`, the panel at the top right of the game
  screen, in the player's color vision.
- `ClientApp`: F6 (`toggle-lagometer`) shows or hides the overlay on every screen; the
  lagometer samples the session while in game and starts afresh at each join. The keys
  at the top of the game screen list F6.
- Tests: the sampling rate, a stall and the 100 samples kept; a sample read from real
  `RttEstimator`, `TrafficCounters` and `NetworkStats`; the levels and labels; the
  scale; and a link as netem makes it: 5 s at 20 ms, 5 s at 170 ms with 10 ms of jitter
  and 1 datagram in 10 lost, then 5 s at 20 ms again: the RTT turns yellow, the jitter
  and loss rise, then every graph is back to Good while the bars still show the bad
  seconds.
- *Play*: F6 in the controls, and a *Lagometer* section with the graphs, their colors and
  how to see them react with `tools/netem.sh`. *Architecture*: `lagometer` and
  `lagometer_view` in the code map.

## Changed
- Changelog: #51 merged on 2026-10-28 (`bb02b79`), with its last commit.

## Commits
- `e76e1c9` feat(client): added the lagometer overlay toggled with f6
- `c43bc0f` feat(tests): added lagometer tests with a degraded then restored link
- `295d0e7` feat(docs): documented the lagometer and updated changelogs

## Notes
- The jitter graph is the snapshots' (`NetworkSummary::snapshot_jitter`), what the player
  sees as stutter; the RTT's own variation stays in `ClientSession::rtt()`.
- The levels are a reading aid, not a rule of the game: nothing changes when a graph
  turns red.
- Verified: 161 tests (`server` configuration against the engine's `develop`, the new ones
  5 times in a row), warnings as errors, clang-format and clang-tidy 22 (the client files
  too, in a configuration with the client), `mkdocs build --strict`, Doxygen without
  warnings. The client was built but not played: no display and no root for netem on the
  machine, so the degradation is checked by the simulated link of the tests.
- `a flooding address is cut off, and the players play on` fails on this machine, with
  this branch and with builds of 2026-10-17 alike (the player's heartbeat is not read
  after the 200 junk datagrams): an environment issue unrelated to this change, which
  touches no server code.
