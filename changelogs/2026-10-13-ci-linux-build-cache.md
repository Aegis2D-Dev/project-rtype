# Incremental Linux builds on pull requests

| | |
|---|---|
| Pull request | [#20](https://github.com/Aegis2D-Dev/project-rtype/pull/20) (opened 2026-10-13) |
| Branch | `ci/linux-build-cache` → `develop` |
| Status | Open |
| Author | @vmbr068 |

## Summary
The Linux *Build* and *ASan* jobs took 4 to 6 minutes, almost all of it compiling the
project and its dependencies from scratch in a fresh container, and *clang-tidy* checked
every source on every pull request. Pull requests now build incrementally in trees
kept on the runner hosts and only lint the sources they change; pushes to `develop` and
`master` still build and lint everything from scratch. Matches Aegis2D-Dev/aegis2d#222.

## Changed
- *Code quality*, Linux *Build* and *ASan* jobs: a *Reuse this runner's source and build
  trees* step keeps a source clone and a build tree in the host's `/var/cache/ci-build`
  (mounted at `/ci-build`), per runner, repository and job, keyed by the clang version.
  It updates the clone with `git checkout`, which only rewrites the changed files, so
  ninja only rebuilds those. Pushes delete the trees first; a broken clone is recreated.
- The following steps run in that clone and build in that tree (`SRC`, `BUILD_DIR`);
  the engine from `FetchContent` lives in the build tree and is still fetched again on
  every configure.
- *Run clang-tidy*: on pull requests, only the changed `.cpp` files, listed with the
  GitHub API (`pull-requests: read`), unless a header, a CMake file or `.clang-tidy`
  changed.
- `tools/lint.sh`: `LINT_FILES` limits the check to the listed sources; without it,
  everything is checked as before.

## Commits
- feat(ci): kept linux build trees and linted changed sources on pull requests
- feat(changelog): updated changelogs

## Notes
- Needs `Aegis2D-Dev/infra-config`, which creates `/var/cache/ci-build` and removes trees
  unused for 14 days, whole.
- The gain shows from the second pull request run on a runner.
- If an incremental build ever misbehaves, delete `/var/cache/ci-build/<runner>` on the
  host, or push to `develop`, which rebuilds from scratch.
