# Game room

| | |
|---|---|
| Pull request | [#73](https://github.com/Aegis2D-Dev/project-rtype/pull/73) (opened 2026-11-04) |
| Branch | `refactor/server-game-room` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#299](https://github.com/Aegis2D-Dev/aegis2d/issues/299) refactor(server): game room |

## Summary
A `Room` now owns a game of the server: its `GameInstance`, the `Simulation` thread that runs
it, the `EventTracker` of what its clients learnt and the names of its players. `GameServer`
keeps the network and a single room, built from the seed and the waves instead of an outside
game. Every server test passes with the room in place. First ticket of the *Network:
multi-instance server* epic (Aegis2D-Dev/aegis2d#298).

## Added
- `rtype/room.hpp`, in `rtype_server`: `Room(seed, waves, rate, after_tick)` builds the game
  then starts its thread, last, so that the thread stops first when the room goes;
  `instance()`, `simulation()`, `events()`, `names()`, for the simulation thread or once it
  stopped.
- `GameServer::room()`.
- Tests: a room runs its game on its own thread, its commands reach the game before the next
  tick, and nothing ticks once it is destroyed.

## Changed
- `GameServer(context, seed, waves, tcp_port, udp_port, rate)` builds its room, instead of
  taking a `GameInstance` that the caller keeps; `simulation()` is `room().simulation()`; the
  event tracker and the names of the players moved to the room.
- `r-type_server`: loads the waves, prints them and the systems of a game, then gives the
  waves to the server.
- Tests: the 9 tests that ran a `GameServer` give it the seed and the waves, and read the
  game with `game_server.room().instance()` where they did; nothing else changed in them.
- *Architecture*: the room.
- Changelog: #63 (`f4ade28`) marked as merged.

## Commits
- `c465760` feat(server): put the game, its thread and its event tracker in a room
- `c4d2174` feat(tests): ran the server tests on the room and added a room test
- feat(docs): documented the game room and updated changelogs

## Notes
- Conflicts to expect with #72, which also changes `GameServer` (the rewinds of lag
  compensation, on the network thread): the rewinds go to the room's simulation once both
  are in.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 on every changed source, `mkdocs build
  --strict`, Doxygen.
- `ctest`: 205 of 206 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
