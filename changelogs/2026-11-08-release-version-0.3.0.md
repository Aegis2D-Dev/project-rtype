# Game version 0.3.0 for the final delivery

| | |
|---|---|
| Pull request | [#106](https://github.com/Aegis2D-Dev/project-rtype/pull/106) (opened 2026-11-08) |
| Branch | `chore/release-version-0.3.0` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#388](https://github.com/Aegis2D-Dev/aegis2d/issues/388) chore(release): tag the deliveries (version part) |

## Summary
The game's first delivery was tagged `v0.2.0`, the version of `project()` then. The final
delivery needs a version of its own: the game is now `0.3.0`, the version of its next tag and
of its packages (`r-type-0.3.0-<system>-<cpu>`). The engine went to `0.2.0` the same way
(Aegis2D-Dev/aegis2d#488).

## Changed
- `project(rtype VERSION 0.3.0)`: the packages, and the version `r-type_server --version` and
  `r-type_client --version` print (`RTYPE_VERSION`), follow.
- Changelog: #105 (`52b093a`) marked as merged.

## Commits
- `f35d97f` feat(build): bumped the game version to 0.3.0 for the final delivery
- feat(changelog): updated changelogs

## Notes
- Merge it before the game's delivery pull request, so the tag `v0.3.0` goes on a `master`
  whose version is `0.3.0`, as CONTRIBUTING asks: the *Release* workflow does not check it,
  but its packages are named after `project()`.
- The game finds the engine with `FetchContent`, not by version: the engine's 0.2 changes
  nothing here.
