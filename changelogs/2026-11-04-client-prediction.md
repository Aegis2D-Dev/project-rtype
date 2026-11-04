# Client-side prediction

| | |
|---|---|
| Pull request | [#67](https://github.com/Aegis2D-Dev/project-rtype/pull/67) (opened 2026-11-04) |
| Branch | `feat/client-prediction` → `develop` (stacked on `feat/protocol-redundant-inputs`, #65, with `refactor/game-ship-movement-step`, #66, merged in) |
| Status | Merged on 2026-11-04 (`c47cd6e`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#292](https://github.com/Aegis2D-Dev/aegis2d/issues/292) feat(client): client-side prediction |

## Summary
The client moves its own ship at once with each input it sends, with the server's own step
(#66), and draws it there instead of 100 ms in the past with the others; it keeps the inputs
the snapshots have not acknowledged yet (#64). With a server 150 ms away, the ship answers
the keys within one frame. Third ticket of the *Network: responsiveness under lag* epic
(Aegis2D-Dev/aegis2d#289).

## Added
- `rtype/ship_prediction.hpp`, in `rtype_client_net`: `ShipPrediction`. `start(ship,
  position)` from a position of the server, `apply(input, delta)` moves the ship with
  `step_ship()` and keeps the input (2 seconds at most, `max_pending`),
  `acknowledge(sequence)` drops the inputs up to it (through the wrap), `stop()`;
  `ship()`, `position()`, `pending()`.
- `ClientSession::prediction()`: started from the own ship's record in the first snapshot
  that has it, again when the player loses a life (the server moved the ship back to its
  spawn point) or gets another ship, stopped once the player is out; each `Input` sent moves
  it, by the server's tick as its simulation computes it; each snapshot drops the inputs its
  player record acknowledges.
- Tests: the prediction moves by the step at each input, held by the play area, keeps and
  drops its inputs through the wrap, keeps 2 seconds of them, and restarts clean. A server
  150 ms away, which sends the world once: one frame after the key goes down, and at each
  frame of the next 150 ms, the predicted ship is where the server's step puts it, while no
  snapshot has come back.

## Changed
- `GameView::draw()` takes the prediction: the own ship is drawn at its predicted position,
  and from its first frame; the other entities as before.
- *Architecture*: the own ship is predicted.

## Commits
- `d20d225` Merge remote-tracking branch 'origin/refactor/game-ship-movement-step' into feat/client-prediction
- `6c838bd` feat(client): predicted the own ship from the inputs sent
- `c3964e0` feat(tests): added prediction tests at 150 ms of round trip
- `e567a25` Merge remote-tracking branch 'origin/refactor/game-ship-movement-step' into feat/client-prediction
- feat(docs): documented client-side prediction and updated changelogs

## Notes
- Stacked on #65, which holds #64 (input sequences); #66 (the shared ship step) is merged in,
  so its commits show in this diff until it reaches `develop`. Merge #64, #65 and #66 first.
- No reconciliation yet (Aegis2D-Dev/aegis2d#293): the prediction is not corrected by the
  snapshots, so it drifts from the server when the server applies the inputs at other ticks
  than the client (late, bunched or lost datagrams) and after a hit that pushes the ship. It
  restarts from the server's position after each death.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 on every changed source (the client's view
  in the client's tree), `mkdocs build --strict`; the prediction tests also under ASan and
  UBSan.
- `ctest`: 221 of 222 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
