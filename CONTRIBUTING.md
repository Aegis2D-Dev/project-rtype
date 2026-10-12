# Contributing to R-Type

The game follows the same workflow as the engine: branches, commit messages, pull
requests, reviews and merges are described in the engine's
[CONTRIBUTING.md](https://github.com/Aegis2D-Dev/aegis2d/blob/develop/CONTRIBUTING.md).
This page lists only what is specific to this repository.

## Tickets

- Every ticket lives in the engine repository, on the team's GitHub Project, including the
  game ones: open new tasks there with the *Task* form.
- A game pull request names its tickets with their repository:
  `Closes Aegis2D-Dev/aegis2d#133` when it completes one, `Refs Aegis2D-Dev/aegis2d#133`
  when the engine part closes it.
- Bugs of the game can be reported here with the *Bug report* form.

## The engine

- Development builds fetch the engine's `develop` (`GIT_TAG develop` in
  `CMakeLists.txt`): a feature merged in the engine reaches the game when the build
  directory fetches it again; if a configure keeps the old engine, delete
  `build/<preset>/_deps/aegis2d-*` and configure again.
- A change that needs a new engine feature is split in two pull requests: the generic part
  in the engine, merged first, then the game part here.
- The engine repository is private: give git your GitHub credentials once, e.g.
  `gh auth setup-git`.
- Game code that reads components (systems, entity creation) goes in the static library
  `rtype_game` or in the binaries, never behind a shared library: the engine's registry
  finds components by `typeid`, which differs across shared libraries with libc++ (macOS).

## Commits

Same shape as the engine: `feat(<scope>): <what was done, past tense>`. Scopes of this
repository: `game`, `server`, `client`, `tests`, `build`, `ci`, `tools`, `docs`,
`changelog`. A pull request usually has three commits: the code, the tests, then the
changelog.

## Before opening a pull request

Install the tools once, in the project `.venv`:

```bash
pip install -r cmake/requirements.txt -r tools/requirements.txt
```

Then check the branch locally, as CI will:

```bash
cmake --preset linux-debug          # or server, macos, windows
cmake --build build/linux-debug
ctest --test-dir build/linux-debug --output-on-failure
tools/format.sh
tools/lint.sh build/linux-debug
```

To hunt memory errors and undefined behavior, configure with the engine's sanitizers:
`cmake --preset linux-debug -B build/sanitize -DAEGIS2D_SANITIZE=ON`, then build and run
`ctest` in `build/sanitize`.

- New behavior comes with Catch2 tests in `tests/` (`rtype_tests`).
- Gameplay values go in `game/include/rtype/tuning.hpp`, collision layers in
  `game/include/rtype/layers.hpp`.

## Pull requests

- **Target**: `develop` (or the previous branch for a stacked pull request).
- **Changelog**: one pull request = one file in [`changelogs/`](changelogs/) plus one row
  in [CHANGELOG.md](CHANGELOG.md), following the template there.
- **Checks**: the *Code quality* workflow must be green: format, build + tests +
  clang-tidy on Linux, build + tests on Windows, and tests under ASan and UBSan.
- **Merge**: with a merge commit, never squash, after one approval.

## Code style

The same files as the engine: [`.editorconfig`](.editorconfig),
[`.clang-format`](.clang-format) (applied with `tools/format.sh`) and
[`.clang-tidy`](.clang-tidy) (run with `tools/lint.sh`, limited to the game's own code);
CI builds with `AEGIS2D_WARNINGS_AS_ERRORS=ON` through `aegis2d::warnings`.
