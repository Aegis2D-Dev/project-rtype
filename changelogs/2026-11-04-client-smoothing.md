# Correction smoothing

| | |
|---|---|
| Pull request | [#69](https://github.com/Aegis2D-Dev/project-rtype/pull/69) (opened 2026-11-04) |
| Branch | `feat/client-smoothing` → `develop` (stacked on `feat/client-reconciliation`, #68) |
| Status | Merged on 2026-11-04 (`5d5ec09`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#294](https://github.com/Aegis2D-Dev/aegis2d/issues/294) feat(client): correction smoothing |

## Summary
A reconciliation that moves the predicted ship no longer makes the drawn ship jump: it glides
to the corrected position at 120 pixels per second, 2 pixels a frame at 60 frames per
second, unless the correction is 32 pixels or more. The lagometer gets a seventh graph, the
corrections per second. At 5 % loss each way, 79 corrections of up to 20 pixels in a minute
never move the drawn ship by more than 7 pixels in a frame, its own 5 included. Fifth ticket
of the *Network: responsiveness under lag* epic (Aegis2D-Dev/aegis2d#289).

## Added
- `ShipPrediction::smooth(seconds)` and `drawn()`: a reconciliation leaves the drawn ship
  where it was, and `smooth()`, called once a frame, closes the gap at `smoothing_speed`
  (120 px/s); from `snap_distance` (32 px), the ship is drawn at its new position at once.
  `start()` and `stop()` leave no gap.
- `LagGraph::Corrections` (`lag_graph_count` is 7): `Lagometer::update()` takes the count of
  corrections so far and samples those since the last sample, per second; yellow from 2,
  red from 10, labelled `CORRECTED 3/S`.
- Tests: a 10-pixel correction drawn over 5 frames, 2 pixels each; one of 32 pixels drawn at
  once; no gap after `start()` or `stop()`. A minute of play through a server 150 ms away
  losing 5 % of the datagrams each way, with redundant inputs, the server holding the last
  input, and a new direction every 8 ticks: 79 corrections, the largest 20 pixels, and never
  more than 7 pixels between two frames. The corrections graph: 3 corrections in 100 ms are
  30 per second, none for the first sample and after `clear()`, its label and levels.

## Changed
- `ClientSession::update()` smooths the prediction by the time since the last call;
  `GameView` draws the own ship at `drawn()`; `ClientApp` gives the corrections to the
  lagometer.
- *Play*: the lagometer has seven graphs, the corrections one in the table. *Architecture*:
  the smoothing.

## Commits
- `8ae30c0` feat(client): smoothed the corrections and graphed them in the lagometer
- `aa4cd37` feat(tests): added smoothing tests at 5 % loss and the corrections graph
- feat(docs): documented correction smoothing and updated changelogs

## Notes
- Stacked on #68, itself on #67, #65 and #64 with #66 merged in: merge them first.
- The lagometer panel was not checked on screen (no display on the machine used): with 7
  graphs it is about 450 pixels high, within the 720 of the play area, and the label of the
  new graph is 14 characters, as wide as the snapshot one.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 on every changed source (the client's view
  and app in the client's tree), `mkdocs build --strict`, Doxygen; the smoothing,
  reconciliation, prediction and lagometer tests also under ASan and UBSan.
- `ctest`: 227 of 228 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
