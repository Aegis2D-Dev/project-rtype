# Release notes from the changelogs: the pull requests since the previous tag

| | |
|---|---|
| Pull request | [#101](https://github.com/Aegis2D-Dev/project-rtype/pull/101) (opened 2026-11-07) |
| Branch | `chore/release-notes` → `develop` |
| Status | Open |
| Author | @Loic-ally |
| Ticket | [Aegis2D-Dev/aegis2d#388](https://github.com/Aegis2D-Dev/aegis2d/issues/388) chore(release): tag the deliveries |

## Summary
The game's GitHub releases say what they deliver, as the engine's do with
Aegis2D-Dev/aegis2d#483: below the text on the packages, the notes list the pull requests of
`CHANGELOG.md` added since the previous tag, newest first, each with its title and a link to
its changelog file at the tag. The *Release* workflow finds the previous tag from the
delivered commit (`v0.2.0` for the final delivery) and reads that tag's `CHANGELOG.md`; a
first release lists every pull request, and a changelog with nothing new stops the release.

## Added
- `tools/release_notes.cmake` (`cmake -P`), the engine's: appends the notes to `OUTPUT`
  from `CHANGELOG`, `PREVIOUS` (empty for a first release) and `BASE_URL`, where the
  changelog's links point.
- Test `release_notes` (ctest): on a changelog of 4 pull requests, one with a stacked branch
  (`→`) and one whose status changed since, the notes since a release that listed 2 of them
  hold the other 2 in order, a first release all 4, and nothing new fails.

## Changed
- *Release* workflow: the publish job checks out the whole history and adds the pull
  requests since the previous tag to the notes.
- *Contributing*: the release notes.
- #49 marked as merged.

## Commits
- `1b725c3` feat(ci): added release notes from the changelogs
- `3190398` feat(tests): added release notes since the previous release
- feat(docs): documented the release notes and updated changelogs

## Notes
- Refs Aegis2D-Dev/aegis2d#388 without closing it: the ticket is done once `develop` is
  delivered to `master` and the final delivery is tagged there. The first delivery was
  tagged `v0.2.0` (the version in `project()`), so the final one needs `project()` bumped
  first, to `0.3.0` for instance: to settle with the team.
- The notes of the final delivery were generated locally from `v0.2.0`'s changelog: 53 pull
  requests.
- Verified on macOS (AppleClang 21): build without warnings; 324 of 326 tests pass (ctest;
  the two flood tests of `server_test.cpp` bind `127.0.0.2`, which macOS lacks without an
  alias, on `develop` as well); clang-format; no C++ changed for clang-tidy;
  `mkdocs build --strict`. This pull request's CI also builds the Linux and Windows
  packages, the workflow having changed.
