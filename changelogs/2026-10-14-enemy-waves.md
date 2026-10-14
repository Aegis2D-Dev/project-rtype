# Enemy waves loaded from a JSON file

| | |
|---|---|
| Pull request | [#22](https://github.com/Aegis2D-Dev/project-rtype/pull/22) (opened 2026-10-14) |
| Branch | `feat/enemy-waves` → `develop` |
| Status | Merged on 2026-10-14 (`5faa07e`) |
| Author | @Loic-ally |
| Ticket | Aegis2D-Dev/aegis2d#146 load enemy waves from JSON |

## Summary
Game part of Aegis2D-Dev/aegis2d#146: the waves of a level are now data. The server reads
`assets/waves/stage1.json` (or the file given with `--waves`) with the engine's
`WaveSchedule`, and a `waves` system creates each enemy when it is due, by type. A new wave
is an edit of the JSON file: no recompilation, which the tests check by writing a file at
run time and playing it. A typo in a type or a value stops the server at start with the file
and the place of the mistake.

## Added
- `assets/waves/stage1.json`: four looping waves of Bydos slaves (a wall, two files, a
  diagonal, a V), all in the lanes where their wave stays on screen; copied next to
  `r-type_server` by `aegis2d_copy_assets()`.
- `rtype/waves.hpp`: `enemy_types` (`bydos_slave`), `default_waves_file()` and
  `load_waves(path)`, which rejects unknown types.
- `spawn_enemy(registry, type, position)`: creates an enemy by the name a waves file uses.
- `ServerGame::play_waves(waves)` and `waves()`; a `waves` system after `spawn` (14
  systems); `spawned()` now lists the enemies of the waves too (it is cleared at the start
  of each tick).
- `r-type_server --waves <file>`: loads and checks the waves, prints their count, or the
  error and exits with 1; an unknown argument prints the usage and exits with 2.
- `tests/waves_test.cpp`: 6 cases: `stage1.json` is valid and in the lanes; a wave written
  at run time spawns its enemies at the right ticks and places (the *done when* of the
  ticket); wave enemies are Bydos slaves; a typo is reported with its place; waves and
  random spawns add up and replay for a seed; a game without waves spawns nothing.

## Changed
- `tests/server_game_test.cpp`: the system order.
- `docs/gameplay.md`: `play_waves()`, the tick table (14 systems), an *Enemy waves*
  section, *Not there yet*. `docs/how-to.md`: a new *Add a wave* section and the enemy
  type steps for waves. `docs/getting-started.md` and `docs/architecture.md`: the waves
  file and `waves.hpp`.
- `CHANGELOG.md` and its file: #21 marked as merged.

## Commits
- feat(game): added enemy waves loaded from a JSON file
- feat(tests): added enemy wave tests
- feat(changelog): updated changelogs

## Notes
- Needs the engine's wave schedules (aegis2d#232) merged in the engine's `develop`.
- The network layer (#112) should call `play_waves()` when a game starts; for now
  `r-type_server` only loads and checks the file.
