# Redundant inputs

| | |
|---|---|
| Pull request | [#65](https://github.com/Aegis2D-Dev/project-rtype/pull/65) (opened 2026-11-04) |
| Branch | `feat/protocol-redundant-inputs` → `develop` (stacked on `feat/protocol-input-sequence`, #64) |
| Status | Merged on 2026-11-04 (`c47cd6e`), with #67 |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#286](https://github.com/Aegis2D-Dev/aegis2d/issues/286) feat(protocol): redundant inputs |

## Summary
Each `Input` datagram carries the input of its tick and the 3 before it, so a lost
datagram's input reaches the server with one of the next three; the server applies each
input once, in order, and ignores those it already applied. With 10 % loss, every input of
a recorded 10-second session reaches the server. The RFC part is Aegis2D-Dev/aegis2d#436.
Tenth ticket of the *Network: data transmission efficiency* epic (Aegis2D-Dev/aegis2d#277),
on the input numbers of #64.

## Added
- `rtype/input.hpp`: `write_inputs()` takes 1 to 4 consecutive inputs, oldest first, and
  writes the newest's sequence, the count and the actions newest first; `read_inputs()`
  gives them back oldest first, or nothing for a count of 0 or above 4, a size other than
  `input_payload_size(count)` or a bit outside `valid_actions`. `max_inputs_per_datagram`
  (4).
- Tests: the RFC example payload and every count, through the wrap; inputs that cannot make
  a datagram (none, 5, a gap, out of order); malformed payloads; the server applies the
  inputs of lost datagrams when the next ones arrive, ignores those of a late datagram, and
  loses only an input whose 4 datagrams are all lost; the client's datagrams carry the 3
  inputs before their own. The bot's dodge script replayed for 10 seconds (600 inputs, then
  3 more) through a link losing 60 of 603 datagrams, twice 3 in a row: the server applies
  all 600, in order, once each, with their actions.

## Changed
- `ClientSession` keeps its last 4 inputs and sends them all in each `Input`.
- `ServerNetwork` passes each input of a datagram not applied yet to `on_input()`, oldest
  first, instead of the newest only.
- `write_input()` and `read_input()` of #64 are replaced by `write_inputs()` and
  `read_inputs()`; `input_payload_size` becomes a function of the count.
- Tests: the hand-written inputs of the server's tests carry their count; the traffic test
  counts 7 bytes of payload per `Input` after the first three.
- *Architecture*: each input goes again with the next three.

## Commits
- `2a04d23` feat(protocol): sent each input again with the next three
- `0c102e0` feat(tests): added redundant input tests with 10 % loss
- `e094c54` Merge remote-tracking branch 'origin/feat/protocol-input-sequence' into feat/protocol-redundant-inputs
- feat(docs): documented redundant inputs and updated changelogs

## Notes
- Stacked on #64: merge it first. The merge brings its test fix of the first CI run.
- The inputs of a datagram are all set before the next tick, so the actions of the inputs a
  lost datagram delayed are applied, but only the newest is held for a whole tick; keeping
  one input per tick on the server would need an input buffer, for client-side prediction.
- An `Input` grows from 21 to 25 bytes: 240 bytes per second more for each player. The
  network benchmark report was not measured again.
- The loss of the recorded session comes from the engine's `core::Random`, the same on every
  platform, seeded so that runs of 3 lost datagrams happen.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 on every changed source, `mkdocs build
  --strict`.
- `ctest`: 214 of 215 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
