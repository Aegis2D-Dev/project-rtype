# Missiles and enemies despawn off screen

| | |
|---|---|
| Pull request | [#16](https://github.com/Aegis2D-Dev/project-rtype/pull/16) (opened 2026-10-13) |
| Branch | `feat/offscreen-despawn` → `develop` |
| Status | Open |
| Author | @Loic-ally |

## Summary
Game part of Aegis2D-Dev/aegis2d#134: missiles and enemies no longer leak. Every missile
fired stayed in the world forever, flying right; now player missiles, enemy missiles and
Bydos slaves carry the engine's `OffscreenDespawn`, and a new `despawn` system, last in the
tick, destroys them once they have left the screen for good. The network layer gets them
through `ServerGame::despawned()`, to tell the clients.

## Added
- `tuning::despawn_margin` (64 px beyond each edge), so an enemy whose path dips off
  screen is kept.
- `despawn` system after `cleanup` (`ecs::despawn_outside()` on the play area) and
  `ServerGame::despawned()`: the entities destroyed this way during the last tick.
- Tests: the system order, a missile destroyed on the exact tick it passes the right edge
  plus the margin and reported once, a minute of fire that never holds more missiles than
  can be in flight (6 at once), a Bydos entering from the right kept until it has crossed
  the screen and an enemy missile leaving on the left, players never despawned, and the
  marker on each entity.

## Changed
- `game/entities.cpp`: `OffscreenDespawn{tuning::despawn_margin}` on player missiles, Bydos
  slaves and enemy missiles; their comments in `entities.hpp`.
- `ServerGame`: the system list in its comment and the events of the tick.
- `docs/gameplay.md`: the tick table (10 systems), a *Leaving the screen* section, the
  entities and tuning tables, `despawned()`; #134 removed from *Not there yet*.
  `docs/how-to.md`: the *Add a system* example no longer uses the despawn system, which
  now exists.

## Commits
- feat(game): added off-screen despawn of missiles and enemies
- feat(tests): added off-screen despawn tests on the server game
- feat(changelog): updated changelogs

## Notes
- Needs the engine's off-screen despawn (aegis2d#212) merged in the engine's `develop`.
- Checked against the engine branch: server build with `-Werror` (25 tests), ASan +
  UBSan, `tools/format.sh`, `tools/lint.sh`, `mkdocs build --strict`, Doxygen.
