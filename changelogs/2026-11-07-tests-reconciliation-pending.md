# Reconciliation test without timing on its pending inputs

| | |
|---|---|
| Pull request | [#105](https://github.com/Aegis2D-Dev/project-rtype/pull/105) (opened 2026-11-07) |
| Branch | `fix/tests-reconciliation-pending` → `develop` |
| Status | Merged on 2026-11-07 (`52b093a`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#486](https://github.com/Aegis2D-Dev/aegis2d/issues/486) fix(tests): reconciliation test fails under load on its pending inputs |

## Summary
The test *reconciled with the snapshots, the own ship ends where the server's is* failed now
and then under ASan and on a loaded runner: it allowed at most 3 inputs not acknowledged yet
at its end, while the client sends one each tick, so how many are in flight depends on how
fast the machine runs. It now checks what that bound meant: every pending input comes after
the last one the server applied, and the list is not full.

## Changed
- Tests: *reconciled with the snapshots...* checks that each pending input is later than the
  last input applied by the server, and that fewer than `ShipPrediction::max_pending` are
  kept, instead of at most 3.
- Changelog: #104 (`b9b3753`) marked as merged.

## Commits
- `b6151ce` feat(tests): checked the pending inputs of the reconciliation without timing
- feat(changelog): updated changelogs

## Notes
- It failed on `develop` under ASan and UBSan (`b0a4a98`, `b9b3753`, `4 <= 3`) and on the
  Windows job of #101; the positions it checks always matched.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors): the
  test passes 20 times out of 20, and 18 times out of 18 under ASan and UBSan with six runs at
  once to load the machine; clang-format, clang-tidy 22 (86 files).
- `ctest`: 314 of 315 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
