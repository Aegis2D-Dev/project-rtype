# Traffic counters

| | |
|---|---|
| Pull request | [#48](https://github.com/Aegis2D-Dev/project-rtype/pull/48) (opened 2026-10-23) |
| Branch | `feat/traffic-counters` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#269 traffic counters |

## Summary
The server and the client count the datagrams and bytes of each session, by direction and
packet type, with the engine's `TrafficCounters` (Aegis2D-Dev/aegis2d#410). The server
logs each player's traffic every 10 seconds; the client exposes its own, for the network
statistics to come. First ticket of the *Network: measurements and lag tools* epic
(Aegis2D-Dev/aegis2d#268): the optimizations of the next epics are measured with it.

## Added
- `ServerNetwork::traffic(token)`: the counters of a bound session, from the `Accept`,
  `Snapshot` and `Event` datagrams sent and the valid `Connect`, `Input`, `Event`,
  `Heartbeat` and `Disconnect` received; `traffic_report_period` (10 s).
- Log line per player every 10 seconds: datagrams and bytes per second over the last whole
  second, by packet type, and the bytes since the player joined.
- `ClientSession::traffic()`: the datagrams sent to the server and those received with the
  session's token.
- Test of a known exchange between a real client and server: both ends count the same
  `Connect`, `Accept`, snapshots and events, inputs of 19 bytes, and the log line.
- *Getting started*: the traffic log line.

## Changed
- `ServerNetwork::update()` reports the traffic before the timeouts, so that a player who
  times out gets its last line.
- Changelog: the shmup weapons pull request is #47 (it was listed as #46, the delivery),
  merged on 2026-10-20 (`ce531df`), with its commits.

## Commits
Added once merged.

## Notes
- Bytes count each datagram with its 18-byte header, without the IP and UDP headers.
- Verified: 145 tests (the new one 10 times in a row), warnings as errors, clang-format and
  clang-tidy 22.
