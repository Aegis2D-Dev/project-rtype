# Code quality CI

| | |
|---|---|
| Pull request | [#10](https://github.com/Aegis2D-Dev/project-rtype/pull/10) (opened 2026-10-12) |
| Branch | `ci/code-quality` → `develop` |
| Status | Merged on 2026-10-12 (`35e9d5f`) |
| Author | @Loic-ally |

## Summary
Every pull request and every push to `develop` and `master` is now built and checked, as
in the engine: formatting, a Linux build of the server, the client and the tests with
warnings as errors, the tests, clang-tidy, a Windows build, and the tests under
AddressSanitizer and UndefinedBehaviorSanitizer. Until now nothing compiled the game
before a merge.

## Added
- `.github/workflows/code-quality.yml`, four jobs:
  - **Format**: `tools/format.sh --check`;
  - **Build, test and lint (Linux)**: Clang + Ninja, `AEGIS2D_WARNINGS_AS_ERRORS=ON`,
    server, client and `rtype_tests`, `ctest`, `r-type_server` and `r-type_client` run
    once, then `tools/lint.sh build`;
  - **Build and test (Windows, MSVC toolchain)**: Visual Studio + clang-cl, `ctest`, both
    binaries with `--version`;
  - **Tests with ASan and UBSan (Linux)**: `AEGIS2D_SANITIZE=ON`, `ctest` with leak
    detection.
- The build jobs fetch the private engine with `EPITECH_MIRROR_TOKEN` (git `insteadOf`
  for `github.com/Aegis2D-Dev/`) and fail with a clear message when it is missing; CPM
  sources are cached between runs.

## Commits
- feat(ci): added code quality workflow
- feat(changelog): updated changelogs

## Notes
- `EPITECH_MIRROR_TOKEN` must be able to read `Aegis2D-Dev/aegis2d` (it already
  dispatches the mirror there). A token dedicated to reading would be cleaner later.
- The game follows the engine's `develop`: an engine change that breaks the game turns
  this CI red on the next run, which is the point.
- Same action versions (pinned by commit) and tool versions as the engine's CI;
  validated with `actionlint`. The commands of each job were run locally against the
  engine's `develop` (server `-Werror`, client with sanitizers, lint).
- Made from `chore/tooling` (#9, not merged yet).
