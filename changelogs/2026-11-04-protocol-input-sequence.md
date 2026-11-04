# Input sequence numbers

| | |
|---|---|
| Pull request | [#64](https://github.com/Aegis2D-Dev/project-rtype/pull/64) (opened 2026-11-04) |
| Branch | `feat/protocol-input-sequence` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#290](https://github.com/Aegis2D-Dev/aegis2d/issues/290) feat(protocol): input sequence numbers |

## Summary
Each `Input` carries its own number, from 0, and each snapshot tells every player the number
of its last input the server applied. The client knows which of its inputs the server has not
applied yet, which client-side prediction and reconciliation will replay. The RFC part is
Aegis2D-Dev/aegis2d#435. First ticket of the *Network: responsiveness under lag* epic
(Aegis2D-Dev/aegis2d#289).

## Added
- `rtype/input.hpp`, in `rtype_protocol`: `InputRecord` (`sequence`, `actions`),
  `write_input()` (3 bytes: the sequence, little-endian, then the actions, bits 5 to 7
  cleared) and `read_input()` (exactly 3 bytes, bits 5 to 7 clear); `valid_actions` and
  `input_payload_size`, which the server and the client shared as copies before.
- `PlayerRecord::input`: the sequence of the player's last input applied, written after its
  score as a `has_input` bit and 16 bits, in full and delta snapshots alike.
- `GameInstance::set_input(player, input, sequence)` keeps the sequence of each player's
  input when it applies it (`PlayerState::input`), forgotten when the player joins again;
  `take_snapshot()` puts it in the player records.
- `ClientSession::last_input()`, the sequence of the last `Input` sent (the low 16 bits of
  `inputs_sent()` before it), and `acknowledged_input()`, the last one the server applied,
  from the record of the client's own player; `RemotePlayer::input`, which a late snapshot
  does not move back (`sequence_greater_than()`, so it follows the wrap).
- Tests: the payload of an input and its malformed forms; the server passes on only inputs
  more recent than the last by their own sequence, through the wrap from 65535 to 0, and
  ignores an older input in a newer datagram; the game keeps the sequence it applied; the
  client world keeps the most recent; the RFC examples of sections 4.7 and 4.8 byte for byte
  with an input; and, against a real `GameServer`, the input the snapshots acknowledge
  catches up five times with the last one sent, never ahead of it nor back.

## Changed
- `ServerNetwork::InputHandler` gets the sequence of the input after its actions; the server
  orders inputs by their own sequence instead of their datagram's.
- `ClientSession` sends `write_input()`'s payload, numbered from 0.
- Tests: inputs sent by hand in the server's tests have the new payload; the sizes of
  snapshots with player records grow by 17 bits per player with an input, 1 without; the
  traffic test counts 3 bytes of payload per `Input`.
- *Architecture*: inputs are numbered and the snapshots acknowledge them.
- Changelog: #63 (`f4ade28`) marked as merged.

## Commits
- `b1725ed` feat(protocol): numbered inputs and acknowledged them in player records
- `58574d1` feat(tests): added input sequence tests and the rfc examples bit by bit
- `6d194b2` feat(docs): documented input sequences and updated changelogs
- `137b9c8` feat(tests): waited for a new input at each step of the acknowledged input test
- feat(changelog): updated changelogs

## Notes
- A client and a server of different builds no longer understand each other's `Input` and
  `Snapshot` payloads; the header keeps version `1`, as with #58.
- The sequence applied is the one of the last input set before the tick of the snapshot: an
  input replaced before a tick ran is never applied on its own. Redundant inputs
  (Aegis2D-Dev/aegis2d#286) build on these sequences.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too (`-DSDL_UNIX_CONSOLE_BUILD=ON`): clang-format, clang-tidy 22 on every
  changed source, `mkdocs build --strict`.
- The first CI run failed under ASan and UBSan: the end-to-end test could reach its five
  steps with 3 inputs sent, the acknowledged input having caught up already; each step now
  waits for a new input first.
- `ctest`: 211 of 212 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
