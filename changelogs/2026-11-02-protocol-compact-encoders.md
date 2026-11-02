# Fixed-point and varint encoders

| | |
|---|---|
| Pull request | [#57](https://github.com/Aegis2D-Dev/project-rtype/pull/57) (opened 2026-11-02) |
| Branch | `feat/protocol-compact-encoders` → `develop` |
| Status | Merged on 2026-11-02 (`1bd6796`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#280](https://github.com/Aegis2D-Dev/aegis2d/issues/280) fixed-point and varint encoders |

## Summary
The protocol gets the encoders of the compact snapshot records, on top of the engine's
`BitWriter` and `BitReader` (Aegis2D-Dev/aegis2d#424): positions in fixed point, velocities
as small signed integers and ids as varints, each with its error bound. Nothing sends them
yet: the quantized snapshot records (Aegis2D-Dev/aegis2d#281) use them next. Third ticket
of the *Network: data transmission efficiency* epic (Aegis2D-Dev/aegis2d#277).

## Added
- `rtype/compact.hpp`:
  - positions to the 1/8 px on 15 bits, from -2048 to 2047.875 (`quantize_position()`,
    `dequantize_position()`, `write_position()`, `read_position()`), off by
    `position_max_error` (1/16 px) at most within the range;
  - velocities in whole pixels per second on 12 bits, from -2048 to 2047
    (`quantize_velocity()`, `write_velocity()`, `read_velocity()`), off by 0.5 px/s at most;
  - out of their range, both are clamped to its ends, and NaN gives 0;
  - ids as varints (`write_varint()`, `read_varint()`, `varint_bits()`): groups of 7 bits,
    lowest first, each followed by a continuation bit, so 8 bits below 128 and 40 at most.
    A varint has a single encoding: a last group of 0, a sixth group or a value above 32 bits
    is rejected and fails the reader.
- Tests: the range covers the play area and the entities beyond its edges; every 1/100 px
  from -100 to 1500 px within 1/16 px; the edges of the play area (0, 1280, 720) from
  1/16 px around them; values out of range, infinities and NaN; the game's speeds exactly,
  a diagonal ship within 0.5 px/s; varints at each size boundary up to 2^32 - 1, at any bit
  offset, truncated at every bit, and the three rejected encodings.
- *Architecture*: the encoders and their bounds, next to the snapshots.

## Changed
- Changelog: #56 merged on 2026-10-29 (`6e43307`).

## Commits
- `6aa89dd` feat(protocol): added fixed-point position, velocity and varint encoders
- `3a0771e` feat(tests): added round trip and bound tests of the compact encoders
- `dc2efd9` feat(docs): documented the compact encoders and updated changelogs
- `01cd0e8` feat(format): fixed format
- feat(changelog): updated changelogs

## Notes
- The position range leaves 767 px beyond the right edge and 2048 px before the left one,
  where entities only go a little beyond the edges before they are removed. With
  `position_bits` at 15, a position and a velocity on both axes take 54 bits.
- Verified with the local engine (`server` preset, Clang 20, warnings as errors): build,
  clang-format, clang-tidy 22 on the new files, `mkdocs build --strict`.
- `ctest`: 180 of 181 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as it has since 2026-10-17 on `develop`, and passes in CI.
