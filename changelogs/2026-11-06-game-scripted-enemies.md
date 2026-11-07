# Enemy types and behaviors in Lua scripts

| | |
|---|---|
| Pull request | [#84](https://github.com/Aegis2D-Dev/project-rtype/pull/84) (opened 2026-11-06) |
| Branch | `feat/game-scripted-enemies` → `develop` |
| Status | Merged on 2026-11-07 (`cac9007`) |
| Author | @Neptnium |
| Ticket | [Aegis2D-Dev/aegis2d#356](https://github.com/Aegis2D-Dev/aegis2d/issues/356) feat(game): scripted enemy behaviors |

## Summary
Enemy types are Lua scripts of `assets/enemies`, run by the server with the engine's
sandboxed scripting (Aegis2D-Dev/aegis2d#464, #465, #466). Each `<type>.lua` file gives its
type's values with `enemy { ... }` (size, health, score, fire rate) and its behavior,
`spawn(self)` and `update(self, dt)`; a new `behaviors` system runs them at each tick, in
place of `sine_motion`. The Bydos slave is the first script: its speed, its weave and its
fire moved there, with the same path and the same shots as before. A new enemy type is a
sprite, a script and a waves entry, with no recompilation: the waves files accept the
types of the scripts.

## Added
- `rtype::EnemyScripts` (`game/include/rtype/enemy_scripts.hpp`): loads every script of a
  directory into one sandboxed state with the engine's `ecs` bindings, creates an enemy by
  type with the components every enemy needs (collider on the enemy layer, health, score,
  ramming, despawn, network id, and a weapon for a type that fires), then runs its
  `spawn`, and its `update` at each tick; `self` holds the enemy's id, where it appeared,
  its age and a number the game draws for it (the random spawns' phase). A wrong file name,
  a script that fails, does not call `enemy { ... }` or gives a wrong value is a
  `core::ConfigError` naming the file and the line (sizes, health and scores out of range
  too). A behavior that fails while the game runs, by an error or by taking the whole
  instruction budget (which `pcall` cannot catch: the C++ side stops it then), is logged
  once and stopped: a failed `update` leaves the enemies of its type at their speed, and an
  enemy whose `spawn` failed is destroyed, so none stands still. `stop()` stops the
  behavior of one enemy.
- `rtype::enemy_types()` and `default_enemies_directory()`: the types of a directory, and
  `assets/enemies`, copied next to `r-type_server`.
- `assets/enemies/bydos_slave.lua`: the Bydos slave (120 px/s, a weave of 48 px every 2 s,
  a shot every 2 s, the first after 1 s).
- `ServerGame::spawn_enemy()` and `enemies()`; `ServerGame` takes the directory of its
  scripts (`assets/enemies` by default) and, with a seed, needs `tuning::random_enemy`.
- `r-type_server` checks the scripts at start, stops on a wrong one, and prints the enemy
  types; each game loads them when it starts.
- Tests: `tests/enemy_scripts_test.cpp`, a new type written by the test and played by a
  waves file, the defaults, every wrong script refused with its file, a failing behavior
  stopped once while the others go on, an endless behavior cut off and stopped, an enemy
  whose spawn fails not created, the enemies destroyed forgotten.

## Changed
- `spawn_bydos_slave()`, `spawn_enemy()`, `rtype::enemy_types` (the array) and the Bydos
  values of `rtype::tuning` are gone: the script has them. `tuning` keeps the Bydos size
  that the clients draw, and the lanes of the random spawns (`bydos_lane_margin`).
- `load_waves()` takes the types it accepts, by default those of `assets/enemies`.
- The tests spawn the Bydos by its script, with the values of `bydos_slave.lua`; the tests
  copy the assets next to `rtype_tests`.
- Documentation: *Add an enemy type* rewritten for the scripts (a sprite, a script, a wave,
  a test); *Gameplay* (enemy scripts, the `behaviors` system, entities and tuning tables),
  *How-to* and the code map of *Architecture*.
- Changelog: #83 marked as merged (`4c7e3f1`).

## Commits
- `838b318` feat(game): moved the enemy types and their behaviors to lua scripts
- `481e797` feat(tests): tested the enemy scripts and spawned the bydos by script
- `c9b6c97` feat(game): stopped the endless enemy behaviors and refused wrong values
- `0fa1569` feat(tests): tested the endless behaviors and the failing spawns
- feat(docs): documented the enemy scripts and updated changelogs

## Notes
- Built and tested in the CI image (clang 18, warnings as errors) against the engine's
  `develop` (`ad3c48c`): 276 of the 276 ctest tests pass; `r-type_server` starts, prints
  `enemies: bydos_slave` and stops on Ctrl+C; `tools/lint.sh` clean on the changed files.
  On macOS (AppleClang 21), the same tests pass but the two flood tests, which bind
  127.0.0.2 that macOS does not route. `tools/format.sh --check`; `mkdocs build --strict`.
- The protocol has one kind of enemy (`EntityKind::BydosSlave`, RFC section 4.1), so the
  clients draw every enemy with the Bydos sprite, at its size; drawing each type with its
  own sprite needs the enemy's type in the protocol, which is not part of this pull request.
