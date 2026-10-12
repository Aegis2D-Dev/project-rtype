# Player missiles with a fire cooldown

| | |
|---|---|
| Pull request | [#8](https://github.com/Aegis2D-Dev/project-rtype/pull/8) (opened 2026-10-12) |
| Branch | `feat/player-weapons` → `develop` |
| Status | Merged on 2026-10-12 (`e2ac724`) |
| Author | @Loic-ally |
| Ticket | Aegis2D-Dev/aegis2d#133 player missiles with fire cooldown |

## Summary
Players now shoot: holding fire spawns a missile from the nose of the ship four times a
second, the rate being enforced by the server with the engine's weapons (aegis2d#207), so
tapping fire or sending fire every tick does not shoot faster.

## Added
- `tuning::player_fire_cooldown` (0.25 s).
- Every player ship gets an `ecs::Weapon` with that cooldown.
- `ServerGame`: a `weapons` system between `play_area` and `collision`: the players
  holding fire with a reloaded weapon fire a missile (`spawn_player_missile()`, credited to
  its shooter); `fired()` lists the missiles of the last tick for the network events.
- 5 test cases: the ticket's case (ten seconds of holding fire give 40 missiles, all owned
  by the shooter), the missile leaving the nose and flying right, tapping, two players,
  and a fired missile killing a Bydos and scoring.

## Changed
- The systems of `r-type_server` and of the system order test now include `weapons`.

## Commits
- feat(game): added player missiles with a fire cooldown
- feat(tests): added fire rate tests on the server game
- feat(changelog): updated changelogs

## Notes
- Needs the engine's weapons (aegis2d#207) merged in the engine's `develop`.
- Made from `feat/server-gameplay` (#7, not merged yet).
- Missiles are not removed when they leave the screen yet: #134.
- Verified against the engine with aegis2d#207: server build with `-Werror` (21 tests),
  libc++ 18 build (21 tests), client build with ASan + UBSan (21 tests), clang-format and
  clang-tidy with the engine's configuration.
