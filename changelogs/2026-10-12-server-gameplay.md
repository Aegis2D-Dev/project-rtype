# Server gameplay on the engine systems

| | |
|---|---|
| Pull request | [#7](https://github.com/Aegis2D-Dev/project-rtype/pull/7) (opened 2026-10-12) |
| Branch | `feat/server-gameplay` → `develop` |
| Status | Open |
| Author | @Loic-ally |
| Tickets | Game side of Aegis2D-Dev/aegis2d#131 player ship and movement · #132 clamp players inside the screen · #138 collision broad phase · #139 AABB collision with layer masks · #140 health and damage · #141 enemy destruction and score · #142 player death, lives and game over |

## Summary
The engine now has generic gameplay systems (movement, play area, collisions, damage,
score, lives); this pull request puts R-Type on top of them. The game gets its own
library with its tuning, its collision layers, the creation of its entities and the
server simulation, which registers every system in tick order. The tickets' "done when"
are checked on the game itself: a ship moves at a fixed speed and stops at the screen
edge, missiles hit only their targets and enemies ignore enemies, a Bydos dies from one
missile and credits its shooter, and the game is over when every player has lost their
three lives.

## Added
- `game/`, static library `rtype_game` used by `r-type_server` and the tests:
  - `rtype/tuning.hpp`: 60 Hz, 1280×720 play area, four players, speeds, sizes, health,
    lives (3), Bydos value (100), collision cell;
  - `rtype/layers.hpp`: `layer::player`, `player_missile`, `enemy`, `enemy_missile` and
    their masks: no friendly fire, enemies ignore enemies and their own missiles;
  - `rtype/entities.hpp`: `player_spawn(slot)` (four slots spread on the left),
    `spawn_player(registry, slot)` (ship with collider, health, ramming damage, lives,
    score, network id), `spawn_player_missile(registry, shooter)` (credited to its
    shooter), `spawn_bydos_slave(registry, position)`, `spawn_enemy_missile(registry,
    position)`;
  - `rtype/server_game.hpp`: `ServerGame`, one game on the server: the registry, the
    scheduler with `steering`, `movement`, `play_area`, `collision`, `damage`, `score`,
    `lives` and `cleanup` in that order, `add_player()` (first free slot, at most four),
    `set_input()`, `tick()`, `game_over()` (only once a player joined), and the events of
    the last tick (`damage()`, `awards()`, `lives_lost()`) for the network.
- `tests/`, `rtype_tests` with Catch2 (fetched through the engine's CPM, as the engine only
  fetches it for its own tests), `RTYPE_BUILD_TESTS` option (ON when the game is the
  top-level project): 16 test cases on the slots, the entities, the layer table and the
  tickets' cases on `ServerGame`.

## Changed
- `r-type_server` links `rtype_game` and, without arguments, prints the server systems
  after the engine modules (`--version` is unchanged for the Epitech mirror check).
- Changelog: #6 (`beb9ac2`) marked as merged, with its commits.

## Commits
- feat(game): added server gameplay on the engine systems
- feat(tests): added gameplay tests on the server game
- feat(changelog): updated changelogs

## Notes
- The game library is static on purpose: the engine's registry finds components by
  `typeid`, which differs across shared libraries with libc++ (macOS); code that reads
  components must live in the binary (see the engine's #196).
- Not wired yet, waiting for the network tickets: the server loop at 60 Hz (#112), the
  inputs from the clients (#113), snapshots and events (#114, #115). `ServerGame` is the
  piece they call.
- Still to do on the gameplay side: missiles fired from the input with a cooldown and
  removed off screen (#133, #134), Bydos movement and random spawns (#135, #136), enemy
  fire (#137), the four ships told apart on the client (#143).
- The play area is the logical resolution the client will use (#85).
- Verified with the engine's `develop` (`6265aae`): server build with `-Werror` (16
  tests, `r-type_server` and `--version`), libc++ 18 build (16 tests), client build with
  ASan + UBSan (16 tests, `r-type_client` under Xvfb), clang-format and clang-tidy with
  the engine's configuration.
