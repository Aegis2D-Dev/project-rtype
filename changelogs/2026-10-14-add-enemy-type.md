# Guide: add an enemy type

| | |
|---|---|
| Pull request | [#23](https://github.com/Aegis2D-Dev/project-rtype/pull/23) (opened 2026-10-14) |
| Branch | `docs/add-enemy-type` → `develop` |
| Status | Merged on 2026-10-14 (`8af5daf`) |
| Author | @Loic-ally |
| Ticket | Aegis2D-Dev/aegis2d#37 how-to add a new enemy type |

## Summary
Aegis2D-Dev/aegis2d#37: the how-to had a short list of steps whose example (a turret with no
velocity and no despawn) would never leave the world. A new page, *Add an enemy type*,
takes a teammate from the sprite to the JSON wave to the spawn, with a complete example
(the Pata-Pata: fast, fluttering, no weapon) whose every snippet was followed as written:
the enemy compiled, its tests passed, `r-type_server` printed the output shown in the page,
and clang-format and clang-tidy found nothing. Two small changes make the guide shorter
and safer to follow.

## Added
- `docs/add-an-enemy.md`: the files to touch, then seven steps (sprite and the name the
  client will load it by, values and their `static_assert`, the creation function with a
  table of the components an enemy needs or may have, its name in `enemy_types` and
  `spawn_enemy()`, a wave in `stage1.json` checked by `r-type_server`, two tests, the
  documentation and the pull request) and a checklist. Listed in the site under *How-to
  guides*.
- Test *every enemy type a waves file can name can be spawned*: each name of
  `rtype::enemy_types` must be created by `spawn_enemy()` as an enemy (collider layer,
  `OffscreenDespawn`, `NetworkId`), so a name added without its branch fails.

## Changed
- `rtype::enemy_types` deduces its size (`std::array enemy_types{...}`): adding a type is
  adding a name.
- The test of `stage1.json` no longer requires every enemy to be a Bydos slave (the loader
  already checks the types), so a wave of a new enemy does not break it.
- `docs/how-to.md`: *Add an enemy type* points to the new page; `docs/gameplay.md`: *Not
  there yet* links to it.
- `CHANGELOG.md` and its file: #22 marked as merged.

## Commits
- `9c16579` feat(game): made the enemy type list grow with its names
- `f345894` feat(tests): checked that every enemy type can be spawned
- `a207699` feat(docs): added the add an enemy type guide and updated changelogs

## Notes
- The example enemy is not added to the game: the guide is the deliverable. Following it
  adds a working Pata-Pata in about 60 lines.
