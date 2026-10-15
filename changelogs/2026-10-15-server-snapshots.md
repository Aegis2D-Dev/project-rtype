# Snapshots broadcast at 20 Hz

| | |
|---|---|
| Pull request | [#30](https://github.com/Aegis2D-Dev/project-rtype/pull/30) (opened 2026-10-15) |
| Branch | `feat/server-snapshots` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#114 broadcast snapshots at 20 Hz |

## Summary
Every bound player receives a snapshot of the world 20 times per second (RFC section 4.3):
the tick, the players and every entity with its kind, position, velocity and frame, split
into datagrams of at most 51 entities. The simulation thread builds them and the network
thread sends them, each datagram acknowledging the player's own. Part of the game server
epic (Aegis2D-Dev/aegis2d#107).

## Added
- `rtype/snapshot.hpp`: `take_snapshot()`, `write_snapshot()` (split into payloads that fit
  a datagram), `read_snapshot()` (malformed payloads rejected), `snapshot_due()`,
  `EntityKind`, `PlayerRecord`, `EntityRecord`, `Snapshot`.
- `tuning::snapshot_rate`: 20 snapshots per second.
- `GameInstance::player_state()`: ship, lives and score of a player; a player out of lives
  keeps the last score of its ship.
- `Simulation`: an optional handler called after each tick.
- `ServerNetwork::send_snapshot()` and `snapshots_sent()`.
- Tests: the RFC example byte by byte, the split, malformed snapshots, the snapshot of a
  game, the 20 Hz rule, the tick handler, and snapshots over real sockets.

## Changed
- `r-type_server`: when a snapshot is due, the simulation thread builds it and posts it to
  the network thread, which sends it to every bound player.
- *Getting started*: what the server does now.
- Changelog: #29 (`0da9f65`) marked as merged, with its commits.

## Commits
- `58a7fae` feat(server): broadcast snapshots at 20 hz

## Notes
- The kind follows the collision layer, which holds one kind each today; a second enemy
  type will need its own kind component.
- Snapshots only update entities: `Spawn` and `Destroy` are the events of #115.
- Verified: 87 tests, 20 times in a row, under ASan and UBSan, the server tests under
  ThreadSanitizer, clang-format and clang-tidy 22, Doxygen, `mkdocs build --strict`, and a
  client holding "right" against `r-type_server`, its ship moving in the snapshots.
