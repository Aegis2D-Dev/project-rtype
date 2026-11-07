# Room rules applied to their games

| | |
|---|---|
| Pull request | [#92](https://github.com/Aegis2D-Dev/project-rtype/pull/92) (opened 2026-11-07) |
| Branch | `feat/server-room-rules` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#307](https://github.com/Aegis2D-Dev/aegis2d/issues/307) feat(server): apply room rules |

## Summary
Each room's games now play by the rules the room was created with (#80): the lives of each
player, the health and spawn rate of the enemies for its difficulty, friendly fire, and the
waves file it names. A room opened for a login keeps the default rules, the game as tuned. Two
rooms with different rules play side by side, each by its own. Ninth ticket of the *Network:
multi-instance server* epic (Aegis2D-Dev/aegis2d#298).

## Added
- `GameRules` (`lives`, `enemy_health_factor`, `spawn_interval_scale`, `friendly_fire`),
  taken by `ServerGame` (after its enemy scripts), `GameInstance` and `Room`, and
  `ServerGame::rules()`; the default ones are the game as tuned.
- `game_rules()`, the gameplay of a room's `RoomRules`: `Easy` and `Hard` take
  `tuning::easy_enemy_health_factor`, `easy_spawn_interval_scale` (health as scripted,
  intervals × 1.5), `hard_enemy_health_factor` and `hard_spawn_interval_scale` (health × 2,
  intervals × 0.6).
- `GameServer::set_waves()`, the waves files a room may name, by name; `r-type_server` reads
  each file next to its own once at start, and leaves out a bad one with a warning.
- Tests: the lives of the rules, a player out after one hit with one life; fewer random Bydos
  on easy and more on hard with the same seed, every enemy with twice the health of its script
  on hard; a
  player's missiles hitting another player with friendly fire only, never its shooter; the
  rules of each difficulty; two rooms with different rules at the same time, their players
  seeing their own lives and each game playing its own rules and waves.

## Changed
- `ServerGame`: players start with the lives of the rules, every enemy that appears gets the
  health of its script times their factor, the random spawns their intervals, and with friendly fire the `weapons`
  system adds `layer::player` to the mask of each missile it fires.
- `GameServer` opens a room with its rules and its waves, and logs them.
- *Gameplay*: the rules of a room and friendly fire. *Architecture*: rooms opened with their
  rules.
- Changelog, with `develop` merged in: #80 (`ce5d219`) and #91 (`7d37054`) marked as merged.

## Commits
- `ca3636f` feat(server): applied the rules of each room to its games
- `b5b03de` feat(tests): added two rooms playing with different rules at the same time
- `4c3ff3c` feat(docs): documented the room rules in game and updated changelogs
- Merge remote-tracking branch 'origin/develop' into feat/server-room-rules

## Notes
- `develop` was merged in once the enemies moved to Lua scripts (#84): the difficulty now
  multiplies the health each script gives its enemy rather than setting it, so a tougher
  scripted enemy stays tougher. The other conflicts were the constructors of `ServerGame` and
  `Room`, which took the enemy scripts and the trace of #87 meanwhile, and the index of
  `CHANGELOG.md`.
- A missile leaves its shooter's nose ahead of it and flies three times faster than a ship,
  so its own missiles never meet it; the test checks it.
- The rooms screen still creates rooms with the default rules: choosing them on the screen is
  not part of the ticket.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors):
  clang-format, clang-tidy 22 (71 files), `mkdocs build --strict`, Doxygen.
- `ctest`: 292 of 293 tests pass; under ASan and UBSan, the test cases of the rules, the rooms
  and the server game pass. *a flooding address is cut off, and the players play on* fails on
  the machine used, as on `develop`, and passes in CI.
