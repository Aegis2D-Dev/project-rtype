# Release workflow and game packages

| | |
|---|---|
| Pull request | [#45](https://github.com/Aegis2D-Dev/project-rtype/pull/45) (opened 2026-10-17) |
| Branch | `ci/release-packages` → `develop` |
| Status | Merged on 2026-10-17 (`8744f02`) |
| Author | @vmbr068 |
| Ticket | None: the release of the playable client (#44) |

## Summary
The game gets packages and releases, like the engine: `cpack` makes one archive per system
with `r-type_server`, `r-type_client`, their assets and the engine libraries they need, and
a tag `v*` on `master` publishes the Linux and Windows packages in a GitHub release. The
version becomes 0.2.0, for the playable client.

## Added
- `.github/workflows/release.yml`: *Package (Linux)* and *Package (Windows)* build the
  packages in Release with the engine's `master`, unpack them and run their binaries from
  another folder; *Publish the release* creates the release of a tag `v*` with both. A pull
  request touching the workflow or `CMakeLists.txt` builds the packages without publishing.
- `CMakeLists.txt`: `aegis2d_install()` of `r-type_server` and `r-type_client` (when built)
  with `assets/`, and `aegis2d_package(NAME r-type)`: `r-type-<version>-<system>-<cpu>.tar.gz`
  (`.zip` on Windows) in `<build>/packages`.
- `CONTRIBUTING.md`: the delivery and release procedure. `README.md`: the packages.

## Changed
- Project version 0.1.0 → 0.2.0 (`--version` of both binaries).
- `changelogs/2026-10-17-client-game.md`: the pull request is #44, merged.

## Commits
- `b2cfa23` feat(ci): added the release workflow and the game packages
- `91932d2` feat(changelog): updated changelogs
- `77ed185` feat(ci): set the run path of the packaged binaries

## Notes
- The engine's `aegis2d_install()` sets the run path of the executables from a variable of
  its own folder, empty in a game fetched with FetchContent: the packaged binaries did not
  find the engine libraries (caught by the workflow). The game sets `INSTALL_RPATH` itself
  until the engine is fixed.
- Verified locally with the server preset: the package holds `bin/r-type_server`,
  `bin/assets/` and the engine libraries, and the unpacked server stops cleanly; the client
  package is checked by the workflow, SDL3 not being available locally.
- Release: once delivered on `master`, `git tag -s v0.2.0 -m "R-Type v0.2.0"` then
  `git push origin v0.2.0`.
