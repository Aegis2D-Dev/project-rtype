# CI build caches on self-hosted runners

| | |
|---|---|
| Pull request | [#18](https://github.com/Aegis2D-Dev/project-rtype/pull/18) (opened 2026-10-13) |
| Branch | `ci/windows-build-cache` → `develop` |
| Status | Merged on 2026-10-13 (`f15554e`) |
| Author | @vmbr068 |

## Summary
The Windows job took 12 to 15 minutes on the self-hosted runners, most of it in the
`cmake` configure: the CPM cache was never restored, so every run cloned all the
dependencies again, and SDL3's configure checks (one MSBuild per check) ran from scratch.
The Windows runners keep their disk between jobs, so the job now reuses what the previous
run left behind. The Linux jobs spent about 2.5 minutes each restoring a 140 MB
CPM archive with `actions/cache`; they now read the sources from a cache kept on the
runner host. Matches Aegis2D-Dev/aegis2d#216.

## Changed
- *Code quality*, Windows job: `CPM_SOURCE_CACHE` points at `C:\cpm-cache`, shared by both
  Windows runners (CPM locks its downloads), and the `actions/cache` step is gone.
- The Windows build tree lives in `$env:RUNNER_WORKSPACE\build`, outside the checkout that
  `actions/checkout` cleans, so configure reuses `CMakeCache.txt` and the build is
  incremental. Each runner has its own `_work`, so the two runners never share it.
  *Run the binaries* reads the executables from there.
- `GITHUB_ENV` (`BUILD_DIR` and `GIT_CONFIG_GLOBAL`) is written with
  `Out-File -Encoding utf8`: the runner uses Windows PowerShell 5.1, where `>>` writes
  UTF-16.
- *Code quality*, Linux *Build* and *ASan* jobs: the container mounts the host's
  `/var/cache/ci-cpm` (created by `Aegis2D-Dev/infra-config`) at `/cpm-cache`, and
  `CPM_SOURCE_CACHE` points there; the *Point CPM* and `actions/cache` steps are gone.

## Commits
- feat(ci): kept windows build tree and cpm cache between jobs
- feat(changelog): updated changelogs
- feat(ci): kept linux cpm cache on the runner hosts
- feat(changelog): updated changelogs

## Notes
- The engine comes from `FetchContent` on `develop`, which fetches again on every
  configure, so the kept build tree still follows the engine. The token is only applied
  through the job's `insteadOf` rewrite and is not stored in `_deps`.
- The first run on each runner fills the caches; the gain shows from the next one.
- The Linux build trees still start from scratch in each container; only the CPM
  sources are kept. The three runners of a host share that cache, and entries unused
  for 30 days are removed.
- If an incremental Windows build ever misbehaves, delete
  `C:\actions-runner*\_work\project-rtype\build` on the VM.
