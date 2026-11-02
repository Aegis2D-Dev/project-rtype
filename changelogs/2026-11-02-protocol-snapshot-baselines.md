# Snapshot acknowledgement and baselines

| | |
|---|---|
| Pull request | [#59](https://github.com/Aegis2D-Dev/project-rtype/pull/59) (opened 2026-11-02) |
| Branch | `feat/protocol-snapshot-baselines` → `develop` (stacked on `feat/protocol-quantized-snapshots`, #58) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#282](https://github.com/Aegis2D-Dev/aegis2d/issues/282) snapshot acknowledgement and baselines |

## Summary
The server keeps the last 32 snapshots sent to each player and knows which ones the player
acknowledged, from the `ack` and `ack_bits` fields every datagram of the client already
carries (RFC section 5.1): the possible baselines of the delta snapshots
(Aegis2D-Dev/aegis2d#283). Fifth ticket of the *Network: data transmission efficiency* epic
(Aegis2D-Dev/aegis2d#277).

## Added
- `rtype/snapshot_history.hpp`: `SnapshotHistory`, per player: `sent()` records a snapshot
  with the sequences of its datagrams and forgets the oldest beyond `capacity` (32);
  `acknowledge()` takes the `ack` and `ack_bits` of a datagram received; a snapshot is
  acknowledged once all its datagrams are. `latest_acknowledged()` (the highest tick),
  `find()`, `acknowledged()`, `size()`.
- `ServerNetwork::snapshots(token)`: the history of a bound player; every valid datagram
  it sends (`Connect`, `Input`, `Event`, `Heartbeat`, `Disconnect`) feeds it.
- Tests: nothing acknowledged before the client says so; the highest tick wins and a late
  acknowledgement does not move it back; a snapshot in three datagrams; `ack_bits` 32
  datagrams back, not 33, and across the sequence wrap; the 33rd snapshot pushing out the
  first. Through a real server, two players acknowledging different snapshots: the server
  knows each one's latest.
- *Architecture*: the history and the acknowledgements.

## Changed
- `ServerNetwork::send_snapshot()` takes the snapshot the payloads encode, optionally, and
  keeps it in each player's history with the sequences of its datagrams. The game server
  passes it, shared by every player.

## Commits
- `d095619` feat(server): kept the last 32 snapshots of each player and their acks
- `28e8c8c` feat(tests): added snapshot acknowledgement and baseline history tests
- feat(docs): documented the snapshot history and updated changelogs

## Notes
- No field is added to the protocol: the acknowledgement fields of RFC section 5.1 already
  say which datagrams the client received, and a client applies every snapshot it receives.
  Acknowledging per datagram also tells a snapshot split in several datagrams that arrived
  whole from one that did not, which a tick in the `Input` would not. RFC section 4 will
  describe the baselines with the delta encoding (Aegis2D-Dev/aegis2d#285).
- Verified with the local engine (`server` preset, Clang 20, warnings as errors): build,
  clang-format, clang-tidy 22 (`tools/lint.sh`), `mkdocs build --strict`.
- `ctest`: 189 of 190 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
