# Gameplay events sent to the players

| | |
|---|---|
| Pull request | [#31](https://github.com/Aegis2D-Dev/project-rtype/pull/31) (opened 2026-10-16) |
| Branch | `feat/server-events` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#115 send gameplay events |

## Summary
Snapshots move the entities, but they are lost now and then and never say that an entity
appeared, died or was shot. The server now sends these as reliable events (RFC section
4.4), resent until each player acknowledges them: the `Spawn` of every entity (a shot is
the `Spawn` of its missile, owned by the shooter's player), its `Destroy` with the reason
that tells the client whether to show an explosion, and the `Death` of each life lost.
A player who joins first gets a `Spawn` of every entity already there (RFC section 4.6).
Part of the game server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `rtype/events.hpp`:
  - `SpawnEvent`, `DestroyEvent`, `DeathEvent` (`GameEvent`), `EventType`,
    `DestroyReason`, `no_player`;
  - `write_event()` and `write_events()`: the bytes of RFC section 4.4 (27, 18 and 15
    bytes); `read_event()`: the checks of a client (RFC section 6.4), unknown kinds and
    reasons kept;
  - `EventTracker`: after each tick, compares the replicated entities with those of the
    previous tick. New ones are `Spawn`s (a ship owned by its player, a missile by its
    shooter's, an enemy by nobody); vanished ones are `Destroy`s at their last position,
    `Killed`, `Consumed`, `Despawned` or `PlayerLeft` from the tick's reports; each life
    lost is a `Death` where the ship was, before the `Destroy` of the ship for the last
    one. `spawns()` lists the world for a player who joins. A new game is followed
    without events.
- `ServerNetwork::send_events()`, to every bound player or to one session, and
  `event_datagrams_sent()`; `update()` resends the events due. A player who leaves 256
  events unacknowledged has lost its session (RFC section 6.2).
- `kind_of_layer()` (`rtype/snapshot.hpp`): the kind of an entity, shared by snapshots and
  events.
- Tests: the three events byte by byte, read back, malformed ones; spawns of a ship, an
  enemy and a shot with their owners; a kill (`Killed` and `Consumed` where they were);
  every death up to the last one and its `Destroy`; a missile off screen; a player who
  leaves; the world sent to a player who joins; and over real sockets, events to every
  bound player or to one, resent until acknowledged.

## Changed
- `r-type_server`: after each tick, the simulation thread posts the tick's events to the
  network thread; a player who joins gets the world as `Spawn`s.
- Architecture page (the events and the snapshots), *Getting started*.
- Changelog: #30 (`b50f80e`) marked as merged, with its commits.

## Commits
- `9e66041` feat(server): sent gameplay events to the players
- `52d071d` feat(docs): documented gameplay events and updated changelogs
- feat(changelog): updated changelogs

## Notes
- `PlayerJoined`, `PlayerLeft` and `GameOver` are not sent yet: the notices of joining and
  leaving players are #118, and `GameOver` comes with the end of a game.
- An entity created and destroyed within one tick (a missile fired point-blank) is never
  seen by the clients.
- The position of a `Destroy` or a `Death` is the one of the previous tick, at most one
  tick of movement away.
- Verified on Linux (NixOS, Clang 20) against the engine's `develop`: 97 tests, also under
  ASan + UBSan and ThreadSanitizer, the new ones 10 times in a row; clang-format and
  clang-tidy 22 on the touched files; `r-type_server` under ASan + UBSan holding 60 Hz with
  the events. No client shows them yet: the client's networking is #124 to #128.
