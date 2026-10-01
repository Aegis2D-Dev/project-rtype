# Bootstrap R-Type on Aegis2D and request the Epitech mirror

| | |
|---|---|
| Pull request | [#1](https://github.com/Aegis2D-Dev/project-rtype/pull/1) |
| Branch | `develop` → `master` |
| Status | Merged on 2026-09-30 (`fe95947`) |
| Author | @vmbr068 |

## Summary
Creates the game project on top of the Aegis2D engine and connects it to the Epitech
delivery pipeline hosted in the engine repository.

## Added
- CMake project `rtype` (CMake 3.30+) fetching Aegis2D with `FetchContent`
  (shallow clone).
- `rtype` executable linking `aegis2d::engine`; `main.cpp` prints the engine core
  module name. Runtime DLLs are copied next to it on Windows (`aegis2d_stage_runtime`).
- `.gitignore` for build folders, Python venv, user presets and compile database.
- `.github/workflows/request-epitech-mirror.yml`: on push to `master` or manual run,
  sends an `epitech-sync-requested` dispatch to Aegis2D-Dev/aegis2d, which assembles,
  builds and publishes both repositories (requires the `EPITECH_MIRROR_TOKEN` secret).

## Changed
- Aegis2D is fetched from `master` (was `main`, then `develop`), matching the branch
  that triggers the mirror.

## Commits
- `77b9767` chore: bootstrap R-Type with Aegis2D FetchContent integration
- `e9db4db` ci: trigger Epitech sync on develop
- `3f0da96` ci: trigger Epitech mirror from master

## Notes
- The engine is tracked by branch (`GIT_TAG master`), not pinned to a tag or commit;
  the Epitech mirror records the exact engine commit in `source-revisions.json`.
- No game code yet: separate server and client executables are planned.
