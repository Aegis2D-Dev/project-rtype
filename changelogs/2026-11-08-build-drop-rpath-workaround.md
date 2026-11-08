# Run path workaround dropped, the engine sets it

| | |
|---|---|
| Pull request | [#108](https://github.com/Aegis2D-Dev/project-rtype/pull/108) (opened 2026-11-08) |
| Branch | `chore/build-drop-rpath-workaround` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#409](https://github.com/Aegis2D-Dev/aegis2d/issues/409) chore(build): drop the run path workaround of project-rtype |

## Summary
For the v0.2.0 release (#45), the game set the run path of `r-type_server` and `r-type_client`
itself, right after `aegis2d_install()`: the engine then left it empty when called from a game
(Aegis2D-Dev/aegis2d#267), and the packaged binaries did not find the engine libraries. The
engine sets it since Aegis2D-Dev/aegis2d#408, delivered on its `master` with `v0.2.0`, which
the *Release* workflow builds the packages with: the game's own setting is gone.

## Changed
- `CMakeLists.txt`: the comment and the `INSTALL_RPATH` of `rtype_installed` after
  `aegis2d_install()` are removed; `aegis2d_install()` sets `$ORIGIN/../<libdir>` on Linux,
  `@executable_path/../<libdir>` on macOS.
- Changelog: #106 (`53d8cf2`) marked as merged.

## Commits
- `a1d7469` feat(build): dropped the run path workaround now set by the engine
- feat(changelog): updated changelogs

## Notes
- Checked on Linux with the engine's `develop` (`server` preset, Clang 20): the installed
  `r-type_server` has the run path `$ORIGIN/../lib64`, finds every engine library next to it
  (`ldd`) and runs from another folder without `LD_LIBRARY_PATH` (`--version`).
- The *Release* workflow of this pull request builds both packages with the engine's `master`
  and runs `r-type_server` and `r-type_client` from another folder, on Linux and Windows: the
  check the ticket asks for. Windows needs no run path, the libraries being next to the
  binaries.
- No Catch2 test: the change is in the packaging, checked by the *Release* workflow.
