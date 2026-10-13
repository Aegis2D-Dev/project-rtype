# Random Bydos spawns on the right edge

| | |
|---|---|
| Pull request | [#19](https://github.com/Aegis2D-Dev/project-rtype/pull/19) (opened 2026-10-13) |
| Branch | `feat/random-enemy-spawns` → `develop` (on top of `feat/bydos-sine-wave`, #17) |
| Status | Open |
| Author | @Loic-ally |
| Ticket | Aegis2D-Dev/aegis2d#136 random enemy spawns on the right edge |

## Summary
Game part of Aegis2D-Dev/aegis2d#136: enemies existed only when code created them. A
`ServerGame` created with a seed now spawns Bydos slaves on its own, at random times and
heights just beyond the right edge, each at a random point of its wave, all drawn from the
seed with the engine's `RandomSpawner`: the same seed gives the same enemies at the same
ticks, on every platform, so a game can be replayed from its logged seed.

## Added
- `ServerGame{seed}` (`std::optional<std::uint64_t>`, none by default), `seed()` and
  `spawned()`: the Bydos spawned during the last tick, for the network layer.
- `spawn` system after `weapons` (12 systems); it is disabled in a game without a seed, so
  tests and scripted levels control the whole world.
- `tuning::bydos_spawn_min_interval` (0.8 s), `bydos_spawn_max_interval` (2 s),
  `bydos_spawn_x` (just beyond the right edge), `bydos_lane_top` and `bydos_lane_bottom`
  (64 and 656: the lines a Bydos can weave around without leaving the screen).
- Tests: no spawn without a seed; a fixed seed gives the same spawns over a minute (another
  seed does not); two minutes of random spawns that all enter at the right edge, keep the
  intervals, stay on screen while weaving and are all despawned or still alive; a Bydos
  placed on its wave by its phase.

## Changed
- `spawn_bydos_slave(registry, line, phase = 0)`: the Bydos starts `phase` seconds into its
  wave, placed where its wave is at that time, so random Bydos do not move in step; with no
  phase it is unchanged.
- `ServerGame`: the system list and the events in its comment.
- `docs/gameplay.md`: `ServerGame{seed}`, `spawned()` and `seed()`, the tick table (12
  systems), an *Enemy spawns* section, the entities and tuning tables, *Not there yet*.
  `docs/how-to.md`: games with and without a seed in tests.

## Commits
- feat(game): added random Bydos spawns from a seed
- feat(tests): added random spawn tests on the server game
- feat(changelog): updated changelogs

## Notes
- Needs the engine's seeded random spawns (aegis2d#214, on top of aegis2d#213) merged in the
  engine's `develop`, and #17 merged first.
- The network layer (#111, #112) picks the seed, e.g. from `std::random_device`, and should
  log it.
