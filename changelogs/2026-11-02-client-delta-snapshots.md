# Delta snapshot decoder

| | |
|---|---|
| Pull request | [#62](https://github.com/Aegis2D-Dev/project-rtype/pull/62) (opened 2026-11-02) |
| Branch | `feat/client-delta-snapshots` → `develop` |
| Status | Merged on 2026-11-02 (`bbc3aa3`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#284](https://github.com/Aegis2D-Dev/aegis2d/issues/284) delta snapshot decoder |

## Summary
The server now sends delta snapshots: each player gets each snapshot encoded against the
latest one it acknowledged (#59, #60), or whole without one. The client rebuilds them from
its baseline and drops, without acknowledging it, a delta whose baseline it no longer has.
With 5 % loss each way over a 30-second game of 4 players, every snapshot the client
receives whole matches the server's. Seventh ticket of the *Network: data transmission
efficiency* epic (Aegis2D-Dev/aegis2d#277).

## Added
- `rtype/snapshot_decoder.hpp`: `SnapshotDecoder`, in `rtype_client_net`: `decode()`
  rebuilds a snapshot with `apply_delta()` from its baseline and the payloads of its tick
  received so far, so a snapshot split in several datagrams is rebuilt as they arrive, in
  any order; it keeps the last 32 (`capacity`, as the server's history) as baselines.
  `decodable()` tells whether the baseline of a payload is kept; a payload without it, or
  against another baseline than the other parts of its tick, is dropped (`dropped()`).
  `find()`, `size()`.
- `ClientSession::snapshots()`: the decoder of the session.
- `ServerNetwork::send_snapshot(snapshot)`: encodes the snapshot with
  `write_delta_snapshot()` for each bound player against `snapshots(token)`'s latest
  acknowledged one, sends it and keeps it in the player's history.
- Tests: a full snapshot rebuilt and used as the baseline of the next; a delta whose
  baseline was pushed out by the 33rd snapshot, or never received, dropped; a snapshot of
  400 entities in several datagrams rebuilt part by part, the last first, and a part
  against another baseline refused. A seeded 30-second game with 4 players firing, through
  a link losing 5 % of the datagrams each way, the server's `SnapshotHistory` and real
  acknowledgements: 569 of 600 snapshots received whole, each equal to the server's once
  quantized, 597 sent as deltas, none dropped. Through sockets: each player gets the next
  snapshot against its own baseline; a client does not acknowledge a delta against a tick it
  never received; in a real game, the client's rebuilt snapshot of the latest acknowledged
  tick holds every entity of the server's.
- *Architecture*: the per-player encoding and the client's decoder.

## Changed
- The `Snapshot` payload is now the delta layout of #60 on both sides: `GameServer` hands
  the snapshot to `send_snapshot(snapshot)`, which encodes it on the network thread, and
  `ClientSession` reads `read_delta_snapshot()` instead of `read_snapshot()`.
- Tests: the fake servers of the client's tests send delta payloads.
- Changelog: #61 marked as merged (`2bd6f69`).

## Commits
- `7b64c4b` feat(client): rebuilt delta snapshots from the acknowledged baselines
- `2edf263` feat(tests): added delta snapshot decoding tests under 5 % loss
- feat(docs): documented the delta snapshot decoder and updated changelogs

## Notes
- A client and a server of different builds no longer understand each other's snapshots,
  as with #58; the header keeps version `1`.
- Not acknowledging a dropped delta is what keeps both sides in step: the server's latest
  acknowledged snapshot is always one the client rebuilt. A late datagram is still decoded
  and kept as a baseline, even when the world ignores its older records.
- Until every part of a split snapshot arrives, the entities of the missing parts keep their
  baseline values in the rebuilt snapshot, and the client applies it to its world; such a
  snapshot is never acknowledged, so it is never a baseline. Snapshots split only beyond
  about 120 entities.
- The RFC section on acknowledgements, baselines and the delta layout is
  Aegis2D-Dev/aegis2d#285; `write_snapshot()` and `read_snapshot()` stay for it and the
  existing tests.
- Verified with the local engine (`server` preset, Clang 20, warnings as errors): build, the
  client built too, `tools/format.sh`, `tools/lint.sh` (clang-tidy 22), Doxygen,
  `mkdocs build --strict`.
- `ctest`: 201 of 202 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
