# Round trip time estimate

| | |
|---|---|
| Pull request | [#50](https://github.com/Aegis2D-Dev/project-rtype/pull/50) (opened 2026-10-27) |
| Branch | `feat/rtt-estimate` → `develop` |
| Status | Merged on 2026-10-27 (`77cc529`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#270](https://github.com/Aegis2D-Dev/aegis2d/issues/270) round-trip time estimate |

## Summary
The server and the client estimate the round trip time of each session, with its jitter,
with the engine's `RttEstimator` (Aegis2D-Dev/aegis2d#412): every datagram now gives its
channel the time it is sent or received. The server logs each player's estimate every 10
seconds next to its traffic; the client exposes its own, for the network statistics to
come. Second ticket of the *Network: measurements and lag tools* epic
(Aegis2D-Dev/aegis2d#268).

## Added
- `ServerNetwork::rtt(token)`: the estimate of a bound session, from the acknowledgements of
  its `Accept`, `Snapshot` and `Event` datagrams.
- `ClientSession::rtt()`: the estimate of the client, from the acknowledgements of its
  `Connect` and `Input` datagrams; the first sample comes with the `Accept`.
- The traffic line of each player ends with `round trip 203 ms (jitter 1 ms)`, or
  `no round trip measured yet`.
- Tests: through a relay that holds every datagram 100 ms each way, as `tools/netem.sh`
  would, a real client and game server; and the first samples and the log line without
  delay.
- *Getting started*: the round trip in the traffic line, and what a sample holds.

## Changed
- The `Connect`, `Accept`, `Input` and `Snapshot` headers are taken with the time they are
  sent, and every datagram received is recorded with the time it arrived.
- Changelog: #48 merged on 2026-10-23 (`f59dd7f`), with its commits.

## Commits
- `8ff5de9` feat(net): measured and logged the round trip time of each session
- `0234d70` feat(tests): added round trip tests through a relay delaying datagrams
- `198eafa` feat(docs): documented the round trip time and updated changelogs
- `0382d49` feat(changelog): merged develop and fixed the changelog conflict
- `c8cabe7` feat(tests): polled without sleeping in the round trip tests
- `03baab8` feat(changelog): updated changelogs
- `2d3f838` feat(changelog): updated changelogs

## Notes
- With 100 ms of delay each way, the server estimates 200 ms and the client 215 to 216 ms.
  A sample holds the time the peer waits before acknowledging: the server until the
  snapshot after the last `Input`, the client until its next `Input`. Both run at 60 Hz, so
  the two waits add up to one input interval (16.7 ms), split between the two sides by the
  phase of the client's inputs against the server's ticks. The ticket accepts one input
  interval over 200 ms; removing the wait would need the acknowledgement delay in the
  protocol header. The test allows 15 ms more for the Windows and sanitizer runners.
- In the graphical client, the network is polled once per frame: a datagram can be recorded
  up to a frame after it arrived, which adds to the client's samples.
- Not played under `tools/netem.sh` (it needs root): the relay of the test stands for it.
- Verified: 147 tests (`server` configuration against the engine's `develop`, the round trip
  ones 19 times in a row), warnings as errors, clang-format and clang-tidy 22.
  `mkdocs build --strict` not run locally (MkDocs not installed); the *Documentation*
  workflow builds it.
