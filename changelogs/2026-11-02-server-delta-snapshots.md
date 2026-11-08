# Delta snapshot encoder

| | |
|---|---|
| Pull request | [#60](https://github.com/Aegis2D-Dev/project-rtype/pull/60) (opened 2026-11-02) |
| Branch | `feat/server-delta-snapshots` → `develop` (stacked on `feat/protocol-snapshot-baselines`, #59) |
| Status | Merged on 2026-11-02 (`ee393d6`), into `feat/protocol-snapshot-baselines`; reached `develop` through #61 |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#283](https://github.com/Aegis2D-Dev/aegis2d/issues/283) delta snapshot encoder |

## Summary
A snapshot can be encoded against a baseline the client acknowledged (#59): only the
changed fields of the entities that changed, the new entities whole, the removed ones by
id, and a full snapshot without a baseline. Over a 20-second game with 4 players firing
and a baseline two snapshots old, the payloads take 91,282 bytes instead of 161,941 (-44 %).
Sixth ticket of the *Network: data transmission efficiency* epic (Aegis2D-Dev/aegis2d#277).

## Added
- `rtype/delta_snapshot.hpp`:
  - `write_delta_snapshot(snapshot, baseline)`: the tick, the players, a bit and the age of
    the baseline (`varint`), then by increasing index, each record's id (index gap and
    generation, as in #58) and a `b2` change: `Updated` with a `b4` mask of the fields
    sent (`x`, `y`, velocity, `frame`), `Added` with every field, or `Removed`. Entities are
    compared once quantized, so a move below the precision of the payload is no change. A
    reused index with a new generation is a removal and an addition; a kind that changes
    is an addition. Split into datagrams like full snapshots.
  - `read_delta_snapshot()`: a payload, refused when malformed (as `read_snapshot()`, plus a
    change of 3, an update of no field, a baseline not older than the tick, or an update or
    removal without baseline).
  - `apply_delta(baseline, parts)`: the snapshot rebuilt from the baseline and the payloads
    of its tick, refused when they mix ticks or baselines, do not match the baseline, or
    update or remove an entity it does not have.
  - `quantized()`: a record as the client gets it.
- Tests: every entity added without a baseline; unchanged entities skipped (11 bytes
  instead of 39 for the example world); moved entities with only their changed fields;
  new, removed and reused indices; 400 moved entities in 2 datagrams, 4 in full; the wrong
  baseline, a missing or mixed part; malformed payloads; and a seeded game whose every
  snapshot is rebuilt exactly from deltas against the snapshot two before.
- *Architecture*: the delta snapshots.

## Changed
- `snapshot.cpp`: the fields shared by both encodings (tick and players, ids, entity
  fields) moved to `protocol/snapshot_fields.hpp`, private to the protocol library; the
  full snapshot layout does not change.

## Commits
- `006a2eb` feat(protocol): added delta snapshot encoder against a baseline
- `fa4bad6` feat(tests): added unchanged, moved, new and removed entity delta tests
- feat(docs): documented the delta snapshots and updated changelogs

## Notes
- The server does not send delta snapshots yet: a client of this branch could not read
  them. The client decoder (Aegis2D-Dev/aegis2d#284) switches both sides at once, the
  server encoding each player's snapshot against `ServerNetwork::snapshots(token)`'s latest
  acknowledged one, and the RFC section follows (Aegis2D-Dev/aegis2d#285).
- `apply_delta()` cannot tell that a part of a split snapshot is missing; such a snapshot
  is never acknowledged, so the server never uses it as a baseline.
- Verified with the local engine (`server` preset, Clang 20, warnings as errors): build,
  `tools/format.sh`, `tools/lint.sh` (clang-tidy 22), Doxygen, `mkdocs build --strict`.
- `ctest`: 197 of 198 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
