# Replace rtype with r-type_server and r-type_client, add presets

| | |
|---|---|
| Pull request | [#4](https://github.com/Aegis2D-Dev/project-rtype/pull/4) (opened 2026-10-02) |
| Branch | `build/executables` → `develop` |
| Status | Open |
| Author | @Loic-ally |
| Tickets | Aegis2D-Dev/aegis2d#3 build: add root CMakeLists with C++20 and both executables · Aegis2D-Dev/aegis2d#5 build: add CMakePresets for Linux and Windows (game side) |

## Summary
Produces the two binaries the subject requires, `r-type_server` and `r-type_client`,
instead of the single `rtype` placeholder, and gives the game the same presets as the
engine.

## Added
- `server/` with `r-type_server`: links only `aegis2d::core`, `ecs` and `net`, so the
  server never depends on SDL3 or OpenGL.
- `client/` with `r-type_client`: links the whole engine.
- Both accept `--version` (prints `r-type_<side> 0.1.0` and exits), used by the Epitech
  mirror check; without arguments they print the engine modules they use, until the real
  server and client land.
- `RTYPE_BUILD_CLIENT` option (ON by default): OFF builds the server only and turns off
  the engine's client modules, so SDL3 and glad are not even downloaded.
- `rtype_executable()` helper: C++26, `RTYPE_VERSION`, binaries at the root of the build
  directory (no per-configuration subfolder), engine DLLs copied next to them on Windows.
- `CMakePresets.json`: `linux-debug`, `linux-release`, `server`, `server-release`,
  `macos`, and `windows` (Visual Studio + clang-cl) with `windows-debug` and
  `windows-release` build presets.

## Changed
- Project version set to 0.1.0.

## Removed
- `main.cpp` and the `rtype` target (moved to `server/main.cpp`).

## Commits
- _not committed yet_

## Notes
- Merge together with Aegis2D-Dev/aegis2d#173, which makes the Epitech mirror run
  `r-type_server --version` and `r-type_client --version` instead of `rtype`.
- Checked on Linux with Clang 18: the assembled submission (engine bundled, as the
  mirror does) builds in Release without warnings and both `--version` calls succeed;
  the `server` preset builds `r-type_server` without downloading SDL3 or glad.
