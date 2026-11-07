# Windows build tree emptied when the engine changes

| | |
|---|---|
| Pull request | [#104](https://github.com/Aegis2D-Dev/project-rtype/pull/104) (opened 2026-11-07) |
| Branch | `fix/ci-windows-stale-build` → `develop` |
| Status | Merged on 2026-11-07 (`b9b3753`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#480](https://github.com/Aegis2D-Dev/aegis2d/issues/480) fix(tests): client world test crashes on Windows CI now and then |

## Summary
The test *the client's entities mirror the server's* crashed on the Windows job of *Code
quality* about one time in two since 2026-11-07. It was no bug of the game: every crash ran on
the runner `cust-aegis2d-win-01`, whose kept build tree held engine DLLs older than the tests,
built against the engine's newer headers. The Windows job now writes the engine commit it
builds with in its tree, and starts from an empty tree when the engine changes.

## Changed
- *Code quality*, Windows job: after the configure, the commit of the engine's sources
  (`_deps/aegis2d-src`) is compared with the one the kept tree was built with
  (`engine-commit.txt`); when they differ, or a kept tree has none, the tree is emptied and
  configured again. Builds stay incremental while the engine does not change.
- Changelog: #99 (`1e595c7`) and #100 (`d41d415`) marked as merged.

## Commits
- `f6ca886` feat(ci): emptied the kept windows build tree when the engine changes
- feat(changelog): updated changelogs

## Notes
- How it was found, on the throwaway branch `ci/debug-480`, on `cust-aegis2d-win-01`:
  - the failed Windows jobs of #93, #95, #96, #100 and `develop` all ran on
    `cust-aegis2d-win-01`; their reruns on `cust-aegis2d-win-02` passed;
  - in a fresh build tree on `cust-aegis2d-win-01`, the test passed 100 times out of 100;
  - in that runner's kept tree, it crashed 20 times out of 20 (`0xC0000005`): its engine
    DLLs dated 14:24, its `rtype_tests.exe` 20:06;
  - once that tree was deleted by hand, the Windows job of `develop` (`d41d415`) passed on
    `cust-aegis2d-win-01`.
- Why the DLLs were not copied again (`aegis2d_stage_runtime()` copies them when a binary is
  linked) was not found; emptying the tree on each change of the engine avoids it whatever
  the cause.
- The *Release* workflow builds in an emptied folder already.
- Checked: the workflow parses as YAML; the step runs on the Windows job of this pull request.
