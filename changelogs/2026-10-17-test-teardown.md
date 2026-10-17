# Network tests that fail cleanly under load

| | |
|---|---|
| Pull request | [#39](https://github.com/Aegis2D-Dev/project-rtype/pull/39) (opened 2026-10-17) |
| Branch | `fix/test-teardown` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | None: fix of the Windows CI run of `develop` 39024840409 |

## Summary
On a loaded Windows runner, 5 tests running a `GameServer` crashed instead of failing, past
their 2-second deadlines. The network tests now wait up to 10 seconds, keep no pending
handler on a constructor's locals, and stop their flood thread even when an assertion
throws, so that a slow runner does not fail correct tests and a failing test fails on its
assertion. Tests only.

## Changed
- Network tests: `test_deadline` of 10 seconds (5 for the client tests) in `pump()`,
  `run_until()` and the waits of the simulation and snapshot tests.
- `Player` (tests): the result of its connection is a member, not a local of its
  constructor.
- "a flood does not slow the tick rate": its flood thread is a `std::jthread`, stopped and
  joined even when an assertion throws.
- Changelog: #38 (`5b4f183`) marked as merged, with its commits.

## Commits
- `4dc3c1b` feat(tests): made network tests fail cleanly under load

## Notes
- With a forced failure, the flood test exited with 134 (`std::terminate`, `0xc0000409` on
  Windows) and now exits with 42, a clean failure. The crash of the other tests could not
  be reproduced off Windows: their trigger, the expired deadline, is removed, but their
  root cause is not proven gone.
- Verified: 122 tests, the network ones 5 times in a row, under ASan, UBSan and
  ThreadSanitizer, clang-format and clang-tidy 22.
