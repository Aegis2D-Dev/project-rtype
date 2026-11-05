# Room lifecycle

| | |
|---|---|
| Pull request | [#75](https://github.com/Aegis2D-Dev/project-rtype/pull/75) (opened 2026-11-05) |
| Branch | `feat/server-room-lifecycle` → `develop` (stacked on `feat/server-room-routing`, #74) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#301](https://github.com/Aegis2D-Dev/aegis2d/issues/301) feat(server): room lifecycle |

## Summary
A room now opens when its first player joins, its game starting on a new thread, and closes
when its last player leaves: its thread is stopped and joined, and its game is gone, so the
next player of that room starts a new one. `r-type_server` plays up to `--max-rooms` rooms at
once, 4 by default; once they are all full, a login is refused as `ServerFull`. Two games of
four bots run side by side at 60 ticks per second each. Third ticket of the *Network:
multi-instance server* epic (Aegis2D-Dev/aegis2d#298), on the rooms of #74.

## Added
- `GameServer::open_rooms()`, `max_rooms()` and `rate()`; the server logs each room opened
  and closed, with its ticks.
- `--max-rooms <count>` (1 to 64) on `r-type_server`, `ServerOptions::max_rooms` and
  `default_max_rooms`.
- Tests: two games of four bots, each room ticking above 50 per second at the same time
  (59.8 on the machine used) and both closing once the bots leave; a room opens with its first
  player in game, not at the login, stays while a player is left, closes with the last, and
  opens again with a new game; the room limit on the command line.

## Changed
- `GameServer(..., rate, max_rooms)`: the last argument is the limit of rooms played at once,
  none of them open at start; the waves are taken by value again, kept for the games to come.
  `room(id)` throws `std::out_of_range` while the room is closed, and so does `simulation()`
  for the first room; `rooms()` became `max_rooms()`.
- `r-type_server` prints the room limit and the rate of each room instead of the rate of its
  single game.
- Tests: the 5 client tests that read the game of the first room take it once their client is
  in game, as the room opens then; the test of the tick rate under a flood puts a player in
  game first, so that a room ticks.
- *Architecture*: rooms opened and closed on demand. *Getting started*: `--max-rooms`.

## Commits
- `1766006` feat(server): opened rooms with their first player and closed them with the last
- `d9d7466` feat(tests): added two games of four bots in rooms opened on demand
- `9e969df` feat(docs): documented rooms opened on demand and updated changelogs
- Merge remote-tracking branch 'origin/feat/server-room-routing' into feat/server-room-lifecycle

## Notes
- Stacked on #74: merge it first. #74 merged in once its two rooms test counted each ship
  once: under ASan in CI, a player of that test bound while the events of a tick were on their
  way got a ship's `Spawn` twice, more often now that a room's game starts with its first
  player.
- A closed room's thread may have posted events and a snapshot for the network thread before
  it stopped; they go to the sessions of that room only, none by then, as a player who joins
  it next is bound by a datagram handled after them.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors):
  clang-format, clang-tidy 22 (69 files), `mkdocs build --strict`, Doxygen.
- `ctest`: 243 of 244 tests pass; under ASan and UBSan, 194 of 195 test cases of the server,
  client and bot. *a flooding address is cut off, and the players play on* fails on the
  machine used, as on `develop`, and passes in CI.
