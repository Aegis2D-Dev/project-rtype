# Follow the engine's develop branch

| | |
|---|---|
| Pull request | — (opened 2026-10-12) |
| Branch | `build/engine-develop` → `develop` |
| Status | Open |
| Author | @vmbr068 |

## Summary
Development builds of the game fetched the engine's `master`, which only changes at
delivery points and still holds the empty module stubs of 2026-09-30: the game could not
use anything merged in the engine since (core, ECS, rendering, network). They now fetch
the engine's `develop`, so each engine feature is available to the game as soon as it is
merged.

## Changed
- `CMakeLists.txt`: `FetchContent_Declare(aegis2d ...)` uses `GIT_TAG develop` instead of
  `master`, with a comment explaining why.

## Commits

## Notes
- The Epitech mirror is not affected: it bundles the engine's `master` through
  `FETCHCONTENT_SOURCE_DIR_AEGIS2D` and never uses this tag. At a delivery, the engine's
  `develop` is merged into its `master` first, then the game's, so the mirror builds a
  matching pair.
- Following a branch means an engine change that breaks the game breaks the next game
  build; pinning a commit SHA instead (without `GIT_SHALLOW`) would make builds
  reproducible at the cost of a pull request for every engine update, which can come once
  the engine settles.
- The engine repository is private: fetching it over HTTPS needs GitHub credentials, for
  instance `gh auth setup-git`.
- Checked on macOS: the `server` preset fetches the engine's `develop` (`6265aae`),
  builds `r-type_server` without warnings, and `--version` works.
