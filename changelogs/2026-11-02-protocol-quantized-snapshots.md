# Quantized snapshot records

| | |
|---|---|
| Pull request | [#58](https://github.com/Aegis2D-Dev/project-rtype/pull/58) (opened 2026-11-02) |
| Branch | `feat/protocol-quantized-snapshots` → `develop` (stacked on `feat/protocol-compact-encoders`, #57) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#281](https://github.com/Aegis2D-Dev/aegis2d/issues/281) quantized snapshot records |

## Summary
Snapshots are packed bit by bit with the encoders of #57, as the RFC now specifies
(Aegis2D-Dev/aegis2d#426, section 4.3): an entity record takes 77 bits instead of 22 bytes,
and a datagram holds about 120 entities instead of 51. Drawn positions stay within 1/16 px
of the server's. Fourth ticket of the *Network: data transmission efficiency* epic
(Aegis2D-Dev/aegis2d#277).

## Added
- `entity_record_bits()`, `entity_record_fixed_bits`, `short_id_gap`, `id_low_bits`,
  `id_high_bits`, `kind_bits`, `frame_bits` and `snapshot_fixed_bits` in
  `rtype/snapshot.hpp`.
- Tests: the RFC example bit by bit (20 bytes); record sizes (77 bits up to a gap of 127,
  a generation of 255 included, 85 bits beyond); a 20-second seeded game with 4 players
  firing whose every record is at most 80 bits and every position, velocity and position
  drawn between two snapshots within its bound (under half a pixel); a world of 300
  entities split 120, 120 and 60; clamped values (infinite, out of range, NaN, frame 200)
  and refused ones (kind 8, player 4); and malformed payloads (cut at every byte, a
  trailing byte, padding bits, 5 players, a non-shortest varint, indices past 24 bits).

## Changed
- `write_snapshot()` and `read_snapshot()` use the packed layout: `b32` tick, `b3` player
  count, players (`b2` number, a bit for the ship then its `varint` id, `b8` lives,
  `varint` score), `b16` entity count, then the entities by increasing index, each with
  the `varint` gap of its id's 24 low bits to the previous record's, the 8 high bits, a
  `b3` kind, `s15` positions in 1/8 px, `s12` velocities in px/s and a `b4` frame.
  `write_snapshot()` fills each datagram with as many records as fit, and throws on a
  kind, player number or player count that does not fit its field.
- Removed `player_record_size`, `entity_record_size`, `snapshot_fixed_size` and
  `max_entities_per_datagram`, which no longer apply.
- The server test of split snapshots: 130 entities give 122 then 8 records.
- *Architecture*: the packed snapshots.

## Commits
- `3220018` feat(protocol): packed snapshot records with quantized fields and id gaps
- `90fae78` feat(tests): added record size, precision and malformed snapshot tests
- `c97c3fa` Merge branch 'feat/protocol-compact-encoders' into feat/protocol-quantized-snapshots
- feat(docs): documented the packed snapshots and updated changelogs

## Notes
- Ids go as an index gap and a generation because the engine's ids carry their reuse
  counter in the 8 high bits: sorted by plain id, two live entities can be 2^24 apart and
  their gap would take 32 bits. The 10-byte bound holds while the indices of two
  consecutive records are less than 128 apart; it did over the whole 20-second game test.
- The header keeps version `1`: builds before and after this change do not understand
  each other's snapshots. See Aegis2D-Dev/aegis2d#426.
- Verified with the local engine (`server` preset, Clang 20, warnings as errors): build,
  clang-format, clang-tidy 22 on the changed files, `mkdocs build --strict`.
- `ctest`: 183 of 184 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
