# Server reconciliation

| | |
|---|---|
| Pull request | [#68](https://github.com/Aegis2D-Dev/project-rtype/pull/68) (opened 2026-11-04) |
| Branch | `feat/client-reconciliation` → `develop` (stacked on `feat/client-prediction`, #67) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#293](https://github.com/Aegis2D-Dev/aegis2d/issues/293) feat(client): server reconciliation |

## Summary
On each newer snapshot, the client puts its predicted ship back where the server had it once
it applied the acknowledged input, and replays the inputs after it with the server's step.
When the server applied the inputs as the client predicted, nothing moves: with a server
150 ms away applying one input per tick, 400 reconciliations correct nothing. A server
correction (a ship pushed 20 pixels) is replayed from the server's position. Fourth ticket
of the *Network: responsiveness under lag* epic (Aegis2D-Dev/aegis2d#289).

## Added
- `ShipPrediction::reconcile(server, acknowledged, delta)`: drops the inputs acknowledged,
  takes the server's position, replays the inputs kept with `step_ship()` and returns how far
  the ship moved; `corrections()` counts the reconciliations that moved it by more than
  `correction_threshold` (1/4 pixel, as snapshots round positions to 1/8 pixel), and
  `last_correction()` gives the last distance, for the smoothing and the lagometer
  (Aegis2D-Dev/aegis2d#294).
- Tests: a simulated server 150 ms away (5 ticks each way) applying one input per tick and
  sending the ship's rounded position every third tick, through every direction and the
  edges for 1200 ticks: no correction, at most 13 inputs kept. A correction injected (inputs
  1 to 3 applied, the ship 20 pixels lower): the ship goes there, inputs 4 to 6 are replayed,
  one correction counted. A reconciliation before any input applied replays them all; none
  before `start()`. Against a real `GameServer`: after holding up and right, then nothing,
  the predicted ship ends within 1/16 pixel of the server's.

## Changed
- `ClientSession`: each snapshot newer than the last one used reconciles the prediction,
  instead of only dropping the acknowledged inputs; a late one changes nothing.
- *Architecture*: the reconciliation and its corrections.

## Commits
- `d320ba4` feat(client): reconciled the predicted ship with each snapshot
- `0857da4` feat(tests): added reconciliation tests without loss and with a correction
- feat(docs): documented server reconciliation and updated changelogs

## Notes
- Stacked on #67, itself on #65 and #64 with #66 merged in: merge them first.
- The server sets every input of a datagram before its next tick (#65) and holds the last one
  until a newer arrives, so late or bunched datagrams still make the server apply an input
  for more or fewer ticks than the client did: those corrections are what the smoothing of
  Aegis2D-Dev/aegis2d#294 hides. The test without loss models a server applying one input
  per tick.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors):
  clang-format, clang-tidy 22 on every changed source, `mkdocs build --strict`; the
  reconciliation, prediction and movement tests also under ASan and UBSan; the end-to-end
  test 15 times in a row.
- `ctest`: 224 of 225 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
