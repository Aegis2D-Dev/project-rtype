# Client world from snapshots and events

| | |
|---|---|
| Pull request | [#41](https://github.com/Aegis2D-Dev/project-rtype/pull/41) (opened 2026-10-17) |
| Branch | `feat/client-snapshots` → `develop` |
| Status | Merged on 2026-10-17 (`d5632dd`) |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#126 apply snapshots to entities |

## Summary
Once in the game, the client keeps a copy of the server's world: events create and destroy
its entities, snapshots move them, with the order rules of RFC section 4.5, so the
client's entities mirror the server's. The wire format of snapshots and events moves to the
library shared by the server and the client. Part of the client networking epic
(Aegis2D-Dev/aegis2d#122).

## Added
- `ClientWorld` (`client/network/`): entities created by `Spawn`, removed by `Destroy`,
  updated by snapshots; late, unknown and destroyed records ignored; destroyed ids forgotten
  after 10 seconds; unknown kinds ignored; the players from the snapshots and the events,
  and `join()` for the local player.
- `ClientSession::world()`.
- `server/include/rtype/event_tracker.hpp` and `world_snapshot.hpp`: the server-only parts
  of the events and snapshots.
- Tests: the world's rules, and a client whose entities mirror a real `GameServer`'s.

## Changed
- `snapshot.hpp`, `snapshot.cpp`, `events.hpp` and `events.cpp` moved to `protocol/` (same
  header names); `EventTracker`, `take_snapshot()`, `snapshot_due()` and `kind_of_layer()`
  stay in the server. `rtype_protocol` depends on `rtype_game`.
- `ClientSession`, in game: snapshots read whole before being applied and acknowledged,
  events read through the `ReliableChannel`, each applied once.
- The acknowledgement test's fake server sends real snapshots.
- *Getting started* and the README layout.
- Changelog: #40 (`d08f5ec`) marked as merged, with its commits and the fix after its
  first CI run.

## Commits
- `b9b244f` feat(client): applied snapshots and events to the client's world
- `5ee809a` feat(changelog): updated changelogs

## Notes
- `GameOver` is not sent by the server yet, so the client does not read it either.
- Verified: 131 tests, the client ones 10 times in a row, under ASan, UBSan and
  ThreadSanitizer, clang-format and clang-tidy 22 on every new and moved file, Doxygen,
  `mkdocs build --strict`; `client/main.cpp` still compiles.
