# Bydos slaves weave in a sine wave

| | |
|---|---|
| Pull request | [#17](https://github.com/Aegis2D-Dev/project-rtype/pull/17) (opened 2026-10-13) |
| Branch | `feat/bydos-sine-wave` → `develop` |
| Status | Open |
| Author | @Loic-ally |
| Ticket | Aegis2D-Dev/aegis2d#135 Bydos slaves move in a sine wave |

## Summary
Game part of Aegis2D-Dev/aegis2d#135: Bydos slaves flew in a straight line. They now weave
up and down around the line they were created on while flying left, with the engine's
`SineMotion`: 48 pixels above and below it, back on it every 2 seconds. A new
`sine_motion` system, right after `movement`, moves them; their path is the same at any
tick rate.

## Added
- `tuning::bydos_wave_amplitude` (48 px) and `tuning::bydos_wave_period` (2 s), and a
  `static_assert` that the amplitude stays within `tuning::despawn_margin`, so a Bydos
  weaving off screen is never despawned before it comes back.
- `sine_motion` system after `movement` (`ecs::apply_sine_motion()`).
- Tests: the Bydos wave (rises first, period, no vertical velocity), its position over
  one wave in the server game, and a Bydos created on the top edge that weaves fully above
  the screen and comes back.

## Changed
- `game/entities.cpp`: `SineMotion` on Bydos slaves; its comment in `entities.hpp`.
- `ServerGame`: the system list in its comment (11 systems).
- Tests that aim a missile at a Bydos now use one that does not weave: they test the score,
  and a weaving Bydos can dodge a missile aimed at its line.
- `docs/gameplay.md`: the tick table (11 systems), a *Bydos slaves* section, the margin in
  *Leaving the screen*, the entities and tuning tables.
- `CHANGELOG.md` and their files: #15 and #16 marked as merged.

## Commits
- feat(game): added the Bydos slave sine wave
- feat(tests): added Bydos sine wave tests on the server game
- feat(changelog): updated changelogs

## Notes
- Needs the engine's sine motion (aegis2d#213) merged in the engine's `develop`.
- Random spawns (#136) can start each Bydos at its own phase with `SineMotion::elapsed`.
