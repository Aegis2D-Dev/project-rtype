# Server trace: the timings of every room, for Perfetto

| | |
|---|---|
| Pull request | [#87](https://github.com/Aegis2D-Dev/project-rtype/pull/87) (opened 2026-11-06) |
| Branch | `feat/server-chrome-trace` → `develop` |
| Status | Open |
| Author | @Neptnium |
| Ticket | [Aegis2D-Dev/aegis2d#368](https://github.com/Aegis2D-Dev/aegis2d/issues/368) feat(core): Chrome trace export |

## Summary
`r-type_server --trace trace.json` times every tick of every room, its systems and the
sending of its snapshots and events, with the engine's profiler, and writes them when the
server stops in the Chrome trace format: the trace opens in Perfetto, a track per room. The
profiler and the trace come with the engine (Aegis2D-Dev/aegis2d#473 and #474, stacked on
#472), which must be merged first: until then, this branch does not build against the
engine's `develop`.

## Added
- `--trace <file>` in `ServerOptions` and the usage line.
- `Simulation`: with `trace`, a profiler frame per tick, the `tick` with each system of the
  game (`ServerGame::set_profiler()`, the scheduler's scopes) and `after_tick`;
  `profiler()` once stopped.
- `Room` and `GameServer`: `start_trace()` traces the rooms opened from then on, a room that
  closes keeps its timings (`room 0 (240 ticks)`), and `save_trace(file)` closes the rooms
  and writes them all; `r-type_server` calls it when it stops and prints the number of
  events.
- Tests: `tests/simulation_test.cpp`, a traced simulation records each tick and its
  systems, an untraced one nothing; `tests/server_test.cpp`, `--trace` read, and a server
  whose room played writing a trace read back with `core::Config`.

## Changed
- Documentation: `--trace` in the server options of *Getting started*.
- Changelog: #83 marked as merged (`4c7e3f1`).

## Commits
- `861a791` feat(server): wrote the timings of the rooms as a chrome trace with --trace
- `ede87a4` feat(tests): added the server trace tests
- feat(docs): documented the server trace and updated changelogs

## Notes
- Built and tested in the CI image (clang 18, warnings as errors) against the engine's
  `feat/core-chrome-trace` through `FETCHCONTENT_SOURCE_DIR_AEGIS2D`: 271 of the 271 ctest
  tests pass; `r-type_server --trace` starts and stops cleanly; `tools/lint.sh` clean on
  the changed files; `tools/format.sh --check`; `mkdocs build --strict`.
- Perfetto: on macOS, a server played by two `r-type_bot` for 4 s wrote 3840 events (240
  ticks); the trace loaded in ui.perfetto.dev, the room as a thread with its ticks and
  their systems nested.
- The CI of this pull request builds against the engine's `develop`, so it fails until
  Aegis2D-Dev/aegis2d#472, #473 and #474 are merged; the pull request is a draft until then.
