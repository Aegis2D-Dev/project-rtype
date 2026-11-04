# Adaptive interpolation delay

| | |
|---|---|
| Pull request | [#70](https://github.com/Aegis2D-Dev/project-rtype/pull/70) (opened 2026-11-04) |
| Branch | `feat/client-adaptive-delay` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#295](https://github.com/Aegis2D-Dev/aegis2d/issues/295) feat(client): adaptive interpolation delay |

## Summary
The client draws the remote entities behind the server by a delay that follows the jitter
of the snapshots, from 50 to 200 ms, instead of a fixed 100 ms. Each snapshot is timed
against the fastest arrival; the delay aims at one snapshot interval plus 1.25 times the
latest of the last 2 seconds. Without jitter it goes down to 50 ms; with 40 ms of jitter it
settles near 95 ms and no entity stops for lack of snapshots in a minute of play. Sixth
ticket of the *Network: responsiveness under lag* epic (Aegis2D-Dev/aegis2d#289).

## Added
- `ClientWorld::interpolation_delay(now)`: the delay at a time. It moves towards its aim
  with time, up at half real time (`delay_increase`), so the drawing slows down but never
  goes back, and down by 10 ms a second (`delay_decrease`), so it speeds up by 1 %.
  `initial_interpolation_delay` (100 ms, before the first snapshots),
  `min_interpolation_delay` (50 ms), `max_interpolation_delay` (200 ms), `jitter_window`
  (2 s).
- Tests: a Bydos moving at 300 px/s, a snapshot every 3 ticks for a minute, 30 ms on the way,
  frames every 4 ms: without jitter the delay ends at 50 ms and the Bydos drawn moves at every
  frame; with up to 40 ms of jitter it never stops either, the delay near 95 ms. An 80 ms
  late snapshot takes the delay to 150 ms at half real time, the drawing still going forward;
  a 1 s late one to 200 ms at most; on time again, the late ones count for 2 seconds, then the
  delay goes down to 50 ms.

## Changed
- `ClientWorld::render_tick()` draws `interpolation_delay(now)` behind instead of the fixed
  `interpolation_delay` constant, which is gone.
- Tests: *remote entities move smoothly between snapshots sent at 20 Hz* expects the 1 % of
  a delay going down, its snapshots arriving on time.
- *Architecture*: how far behind the entities are drawn.
- Changelog: #63 (`f4ade28`) marked as merged.

## Commits
- `79998e0` feat(client): followed the snapshots' jitter with the interpolation delay
- `3c32d51` feat(tests): added interpolation delay tests with and without jitter
- feat(docs): documented the adaptive interpolation delay and updated changelogs

## Notes
- The lateness is measured where the client already times the ticks (`server_tick()`),
  against the fastest arrival, rather than with `NetworkStats`' jitter, which follows the
  intervals between snapshots and not how late each one is.
- Not tried through `tools/netem.sh` (needs root on the machine used): the tests replay the
  arrivals of a link with and without jitter.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 on every changed source, `mkdocs build
  --strict`, Doxygen; the world tests also under ASan and UBSan.
- `ctest`: 207 of 208 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
