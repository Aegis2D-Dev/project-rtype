# Shared ship movement step

| | |
|---|---|
| Pull request | [#66](https://github.com/Aegis2D-Dev/project-rtype/pull/66) (opened 2026-11-04) |
| Branch | `refactor/game-ship-movement-step` → `develop` |
| Status | Merged on 2026-11-04 (`c47cd6e`), with #67 |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#291](https://github.com/Aegis2D-Dev/aegis2d/issues/291) refactor(game): shared ship movement step |

## Summary
The movement of a player ship for one tick (the velocity of its input, the move, the
clamping to the play area) is now a function of `rtype_game`, `step_ship()`, built from the
same two pieces the server's systems use. A test runs the server's game tick by tick and
finds the ship exactly where the step puts it, so the client can predict its own ship with
it (Aegis2D-Dev/aegis2d#292). Second ticket of the *Network: responsiveness under lag* epic
(Aegis2D-Dev/aegis2d#289).

## Added
- `rtype/ship_movement.hpp`: `ship_velocity(input)` (held directions at
  `tuning::player_speed`, diagonals as fast), `keep_ship_in_area(position)` (the ship's box,
  `tuning::player_size`, inside `ServerGame::play_area()`), `step_ship(position, input,
  delta)`, which gives a `ShipMotion` (position, velocity) with the arithmetic of the
  server's systems, and the systems `steer_ships()` and `keep_ships_in_area()`.
- Tests: the step on its own, at the player speed in the 8 directions, opposite directions
  cancelling out, nothing held or fire only; held against an edge and every corner, and a
  ship far outside brought back; and the server's game over 3000 ticks of every direction,
  against the edges and away, the ship's position and velocity equal to the step's at every
  tick.

## Changed
- `ServerGame`: the `steering` and `play_area` systems are `steer_ships()` and
  `keep_ships_in_area()` instead of the engine's `shmup::steer_player_ships()` and
  `shmup::clamp_player_ships()`; the speed comes from `tuning::player_speed`, with which every
  ship is spawned, instead of each ship's `shmup::PlayerShip::speed`. Same names, same order:
  the server's tests pass unchanged.
- *Gameplay*: the systems of a tick name the new functions; `step_ship()` and why it matches
  the server.
- Changelog: #63 (`f4ade28`) marked as merged.

## Commits
- `320e995` feat(game): moved player ships with a shared one-tick step
- `0b50c5e` feat(tests): added ship step tests against the server's ticks
- `d8c2399` feat(docs): documented the shared ship step and updated changelogs
- `c4dc4bb` feat(tests): kept the inputs of the ship step test alive
- feat(changelog): updated changelogs

## Notes
- The client does not call the step yet: client-side prediction (Aegis2D-Dev/aegis2d#292)
  will, on the inputs it keeps.
- Positions are compared exactly, not approximately: the step adds `velocity * delta` then
  clamps, as `ecs::apply_velocity()` and the `play_area` system do, in the same float
  operations.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 on every changed source, `mkdocs build
  --strict`, Doxygen.
- The first CI run failed under ASan: the test kept its inputs as `std::initializer_list`s
  in a vector, whose arrays end with the statement that builds it; it keeps
  `ecs::PlayerInput`s now, checked under ASan and UBSan on the machine used.
- `ctest`: 207 of 208 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
