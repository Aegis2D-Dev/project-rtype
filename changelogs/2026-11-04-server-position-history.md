# Entity position history

| | |
|---|---|
| Pull request | [#71](https://github.com/Aegis2D-Dev/project-rtype/pull/71) (opened 2026-11-04) |
| Branch | `feat/server-position-history` → `develop` |
| Status | Merged on 2026-11-04 (`1801efc`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#296](https://github.com/Aegis2D-Dev/aegis2d/issues/296) feat(server): entity position history |

## Summary
The server keeps where each enemy was at each of the last 30 ticks (500 ms), recorded at the
end of every tick, and reads the position of an enemy at a past tick: what lag compensation
(Aegis2D-Dev/aegis2d#297) will check a player's missiles against. Seventh ticket of the
*Network: responsiveness under lag* epic (Aegis2D-Dev/aegis2d#289).

## Added
- `rtype/position_history.hpp`, in `rtype_game`: `PositionHistory`. `record(tick,
  registry)` keeps the center of every entity of the `enemy` collision layer, by entity
  handle (a reused slot is another enemy), and forgets ticks beyond `capacity` (30); a tick
  not newer than the last starts again (a new game). `position(entity, tick)`,
  `enemies(tick)`, `oldest()`, `newest()`, `size()`, `clear()`.
- `tuning::position_history_ticks`: 30, half a second at 60 ticks per second.
- `ServerGame::history()`: recorded at the end of each `tick()`, under the number `ticks()`
  then has.
- Tests: a weaving Bydos over 2 seconds of a game: at each of the last 30 ticks the history
  gives the position the tick left, nothing for older or later ticks. Only enemies are
  kept, a player is not; a destroyed enemy is still at the ticks it lived, not after, and a
  new enemy in its slot has no past. A raw registry over 100 ticks keeps the last 30, and a
  tick going back starts the history again.

## Changed
- *Gameplay*: `history()` in the calls of the server game.
- Changelog: #63 (`f4ade28`) marked as merged; with `develop` merged in, #64 (`ac70cdc`), #65
  and #66 (with #67), #67 (`c47cd6e`), #68 (`2266e33`) and #69 (`5d5ec09`) too.

## Commits
- `1b161ea` feat(server): kept the positions of the enemies over the last 500 ms
- `869e1be` feat(tests): added position history tests at past ticks
- `0a6c64a` feat(docs): documented the position history and updated changelogs
- `c5c3d3b` Merge remote-tracking branch 'origin/develop' into feat/server-position-history
- feat(changelog): fixed pr numbers and merge status

## Notes
- Each tick costs a copy of the enemies' positions (a handle and two floats each) and a
  sort; with the tens of enemies of a wave, a few hundred bytes a tick.
- `PositionHistory::record()` takes the registry mutable only because the engine's
  `Registry::view()` has no const form; it reads nothing else.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 on every changed source, `mkdocs build
  --strict`, Doxygen.
- `develop` merged in once #64 to #69 were in (a conflict in the index of `CHANGELOG.md`
  only); built and tested again: 230 of 231 tests pass.
- `ctest`: 207 of 208 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
