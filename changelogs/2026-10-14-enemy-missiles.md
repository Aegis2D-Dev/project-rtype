# Bydos slaves fire enemy missiles

| | |
|---|---|
| Pull request | [#21](https://github.com/Aegis2D-Dev/project-rtype/pull/21) (opened 2026-10-14) |
| Branch | `feat/enemy-missiles` → `develop` |
| Status | Merged on 2026-10-14 (`65d79b7`) |
| Author | @Loic-ally |
| Ticket | Aegis2D-Dev/aegis2d#137 enemy missiles |

## Summary
Game part of Aegis2D-Dev/aegis2d#137: enemy missiles existed (`spawn_enemy_missile()`) but
nothing fired them. Bydos slaves now fire on their own with the engine's `AutoTrigger`: a
missile to the left every 2 seconds, the first one second after they spawn, once they are on
screen. Random Bydos draw an extra delay from the seed, so they do not fire in step and a
seed still replays the same game. Enemy missiles hit players, who lose a life.

## Added
- `tuning::bydos_fire_cooldown` (2 s) and `tuning::bydos_first_shot` (1 s).
- `fire_enemy_missile(registry, shooter)`: an enemy missile on the left side of the
  shooter, owned by it.
- `enemy_fire` system after `weapons` (13 systems): `ecs::trigger_auto_weapons()`, then one
  missile per enemy whose center is on screen; `ServerGame::enemy_fired()` lists them.
- Tests: a Bydos fires to the left at 1, 3 and 5 s; an enemy missile costs a player a life
  (the *done when* of the ticket); player and enemy missiles ignore each other; random
  Bydos fire at their own times, the same for a seed; enemies off screen do not fire; the
  Bydos weapon and the position of the missile.

## Changed
- `spawn_bydos_slave(registry, line, phase, first_shot)`: Bydos get a `Weapon` and an
  `AutoTrigger`; `first_shot` defaults to `tuning::bydos_first_shot`. The `spawn` system
  draws it between 1 and 3 s.
- `ServerGame`: the system list and the events in its comment.
- Tests: the system order, and the two-minute random spawn test counts the enemy missiles.
- `docs/gameplay.md`: the tick table (13 systems), an *Enemy fire* section, `enemy_fired()`,
  the entities and tuning tables, *Not there yet*.
- `CHANGELOG.md` and its file: #19 marked as merged.

## Commits
- feat(game): added Bydos slaves firing enemy missiles
- feat(tests): added enemy missile tests on the server game
- feat(changelog): updated changelogs

## Notes
- Needs the engine's automatic weapons (aegis2d#231) merged in the engine's `develop`.
- The random spawns now draw two numbers per Bydos (phase, first shot): a seed gives other
  spawns than before this change, still the same ones at every run.
