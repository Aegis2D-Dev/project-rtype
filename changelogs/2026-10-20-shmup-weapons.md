# Weapons from the engine's shmup module

| | |
|---|---|
| Pull request | [#47](https://github.com/Aegis2D-Dev/project-rtype/pull/47) (opened 2026-10-20) |
| Branch | `feat/shmup-weapons` → `develop` |
| Status | Merged on 2026-10-20 (`ce531df`) |
| Author | @Loic-ally |
| Ticket | Follow-up of Aegis2D-Dev/aegis2d#315 shmup module and weapons |

## Summary
The engine moved the weapons out of its ECS into the new `aegis2d::shmup` module
(Aegis2D-Dev/aegis2d#394). R-Type now uses the module's names and headers, so the engine can
drop the old ECS aliases. Nothing changes in the game.

## Changed
- `rtype_game` links `aegis2d::shmup`.
- `spawn_player()` and `spawn_bydos_slave()` give `shmup::Weapon` and `shmup::AutoTrigger`;
  the `weapons` and `enemy_fire` systems call `shmup::trigger_weapons()` and
  `shmup::trigger_auto_weapons()`.
- Tests use the shmup names; README, home page and *Gameplay*: the server's engine modules.
- Changelog: #45 (`8744f02`) marked as merged, with its commits.

## Commits
- `d91f21c` feat(shmup): used the engine's shmup module for the weapons
- `8301cb4` feat(shmup): tested the weapons with the shmup names
- `be98664` feat(shmup): documented the shmup module in the game

## Notes
- Needs the engine pull request merged first: before it, `aegis2d/shmup/` does not exist.
- Verified: build with warnings as errors and the 144 tests against the engine branch, and a
  build against that engine with its old ECS aliases removed; clang-format and clang-tidy 22,
  `mkdocs build --strict`.
