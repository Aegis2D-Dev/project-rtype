# Ship controls and player ships from the engine's shmup module

| | |
|---|---|
| Pull request | [#49](https://github.com/Aegis2D-Dev/project-rtype/pull/49) (opened 2026-10-27) |
| Branch | `refactor/shmup-ship-controls` → `develop` |
| Status | Merged on 2026-10-27 (`e3d3317`) |
| Author | @Loic-ally |
| Ticket | [Aegis2D-Dev/aegis2d#318](https://github.com/Aegis2D-Dev/aegis2d/issues/318) ship controls and player styles in the shmup module |

## Summary
The engine moved the player ships and their controls out of its ECS and platform modules into
`aegis2d::shmup` (Aegis2D-Dev/aegis2d#413), without aliases, so that no other engine module
mentions ships. R-Type now uses the module's names and headers. Nothing changes in the game.

## Changed
- The `steering` and `play_area` systems call `shmup::steer_player_ships()` and
  `shmup::clamp_player_ships()`; `spawn_player()` uses `shmup::spawn_player_ship()`.
- The client's preferences hold a `shmup::ShipControls`; the connect screen's control rows
  are `ecs::Action`s (`platform::ShipAction` is gone, it had the same bits).

## Commits
- `2d4331b` feat(game): used the ship controls and player ships of the shmup module
- feat(changelog): updated changelogs

## Notes
- Needs the engine pull request merged first: before it, `aegis2d/shmup/ships.hpp` and
  `aegis2d/shmup/ship_controls.hpp` do not exist, so this pull request's CI fails until then;
  after it, `develop` does not build without this one, so merge them together.
- Verified on macOS (AppleClang) against the engine branch: build with warnings as errors,
  the 145 tests, clang-format and clang-tidy 22 on the changed files.
