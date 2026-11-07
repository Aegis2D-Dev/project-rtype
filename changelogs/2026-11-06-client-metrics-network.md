# Memory and network numbers in the frame metrics

| | |
|---|---|
| Pull request | [#86](https://github.com/Aegis2D-Dev/project-rtype/pull/86) (opened 2026-11-06) |
| Branch | `feat/client-metrics-network` → `develop` |
| Status | Open |
| Author | @Neptnium |
| Ticket | [Aegis2D-Dev/aegis2d#366](https://github.com/Aegis2D-Dev/aegis2d/issues/366) feat(tools): memory and network in the metrics overlay |

## Summary
The frame metrics of `r-type_client` (F12) show the memory the client uses and, while
connected to a game, the network numbers of the lagometer: round trip, jitter, loss,
traffic, snapshot interval and corrections, with or without the lagometer's graphs (F6).
The memory line and the game's lines come with the engine (Aegis2D-Dev/aegis2d#472), which
must be merged first: until then, this branch does not build against the engine's
`develop`.

## Added
- `rtype::lag_lines(sample)` (`rtype/lagometer.hpp`): the figures of a lagometer sample as
  three lines of the overlay, `RTT 42 MS  JITTER 3 MS  LOSS 0.5 %`, `IN 2.5 KB/S  OUT 1.0
  KB/S` and `SNAPSHOTS 50 MS  CORRECTED 2/S`.
- `client_app`: the latest sample's lines given to the metrics overlay in game, none on the
  menus.
- Tests: `tests/lagometer_test.cpp`, the lines of a sample.

## Changed
- Documentation: the memory and the network numbers in the *Frame metrics* section of
  *Play*.
- Changelog: #83 marked as merged (`4c7e3f1`), then #84, #85, #88, #89 and #90
  (`cac9007`, `92be473`, `61a7cbb`, `8d64bf8`, `c16bd94`).

## Commits
- `ec7865b` feat(client): showed the network numbers in the frame metrics
- `696939a` feat(tests): added the network lines of the frame metrics test
- `13d27dc` feat(docs): documented the network numbers in the frame metrics and updated changelogs
- `217699b` Merge remote-tracking branch 'origin/develop' into feat/client-metrics-network
- feat(changelog): updated changelogs

## Notes
- Built and tested in the CI image (clang 18, warnings as errors) against the engine's
  `feat/tools-metrics-memory-network` through `FETCHCONTENT_SOURCE_DIR_AEGIS2D`: 269 of the
  269 ctest tests pass; `tools/lint.sh` clean on the changed files; `tools/format.sh
  --check`; `mkdocs build --strict`. Built on macOS (AppleClang 21) too; the client's
  window does not draw while hidden behind others on this machine, so the screenshot of
  the docs was not taken again.
- The CI of this pull request builds against the engine's `develop`: it failed until
  Aegis2D-Dev/aegis2d#472 were merged, on 2026-11-07. With `develop` merged in since, built on macOS
  (AppleClang 21) against the engine's `develop` (`f6bf491`): 293 of the 295 ctest tests pass,
  the two flood tests failing on this machine only (they bind UDP to 127.0.0.2, not routed
  on macOS without an alias).
