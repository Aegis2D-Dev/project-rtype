# Enemy scripts reloaded while the game runs

| | |
|---|---|
| Pull request | [#85](https://github.com/Aegis2D-Dev/project-rtype/pull/85) (opened 2026-11-06) |
| Branch | `feat/game-reload-behaviors` → `feat/game-scripted-enemies` (stacked on #84) |
| Status | Merged on 2026-11-07 (`92be473`) |
| Author | @Neptnium |
| Ticket | [Aegis2D-Dev/aegis2d#357](https://github.com/Aegis2D-Dev/aegis2d/issues/357) feat(game): reload behaviors on change |

## Summary
In a debug build, each game of the server reads an enemy script again when its file
changes, with the engine's `core::HotReload`: editing a pattern changes the enemies of the
running game within a quarter of a second, no restart. The enemies alive run the new
behavior from the next tick, with what their `self` table holds; a script that does not
load is logged and the type keeps the one it had. Stacked on #84, the enemy scripts.

## Added
- `EnemyScripts::reload(type)`: runs the type's file again in the game's state; the new
  `spawn` and `update` are used from then on, the new values for the enemies spawned next,
  and a behavior stopped by a failure runs again once its script is fixed. A wrong script
  (one that does not load, does not call `enemy { ... }` or gives a wrong value) is a
  `core::ConfigError`, and the type keeps its definition.
- `EnemyScripts::watch(hot)`: one file watched per type, reloaded by `hot.update()`, which
  logs a reload that fails; `enemy <type> reloaded` is logged when one succeeds.
- `ServerGame` takes `hot_reload` (`core::hot_reload_by_default`: on in debug builds) and
  looks at the scripts at most every 250 ms, at the start of `tick()`.
- Tests: `tests/enemy_scripts_test.cpp` (5 test cases): a reloaded script changes the
  enemies alive and the next ones, wrong scripts refused with the old one kept, a broken
  behavior running again once fixed, a running game whose pattern is edited on disk (and
  then broken, played on with the last one that worked), and a game without hot reload.

## Changed
- Documentation: *Add an enemy type*, how to change a script while the game runs;
  *Gameplay*, the reload in the enemy scripts.

## Commits
- `37e017a` feat(game): reloaded the enemy scripts when their files change
- `939088a` feat(tests): added the enemy scripts reload tests
- feat(docs): documented the enemy scripts reload and updated changelogs

## Notes
- Built and tested in the CI image (clang 18, warnings as errors) against the engine's
  `develop` (`ad3c48c`): 281 of the 281 ctest tests pass; `tools/lint.sh` clean on the
  changed files. On macOS (AppleClang 21), the same tests pass but the two flood tests,
  which bind 127.0.0.2 that macOS does not route. `tools/format.sh --check`; `mkdocs build
  --strict`.
- The server reads the copies of the scripts next to it (`build/<preset>/assets/enemies`):
  edit those, or edit `assets/enemies` and build, which copies them.
- The reload follows the engine's convention, on in debug builds only: a release server
  does not watch its files.
