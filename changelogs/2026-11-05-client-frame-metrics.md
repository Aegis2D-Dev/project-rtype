# Frame metrics in the client, with F12

| | |
|---|---|
| Pull request | [#83](https://github.com/Aegis2D-Dev/project-rtype/pull/83) (opened 2026-11-05) |
| Branch | `feat/client-frame-metrics` → `develop` |
| Status | Merged on 2026-11-06 (`4c7e3f1`) |
| Author | @Neptnium |
| Ticket | [Aegis2D-Dev/aegis2d#365](https://github.com/Aegis2D-Dev/aegis2d/issues/365) feat(tools): frame metrics overlay |

## Summary
F12 shows the engine's frame metrics overlay over every screen of `r-type_client`: the
frames per second, the mean and longest frame times, the entities in game, the sprites and
draw calls of the frame before, and a bar per frame. `--metrics` shows it from the start.
The overlay itself comes with the engine (Aegis2D-Dev/aegis2d#460), which must be merged
first: until then, this branch does not build against the engine's `develop`.

## Added
- `client_app`: the `toggle-metrics` action on F12, logged when it changes; a
  `render::FrameMetrics` ticked once a frame and a `render::MetricsOverlay` drawn last on
  every screen, at the top center (clear of the ships, which start on the left, and of the
  lagometer on the right), with the entities of the client's world in game and none on the
  menus.
- `--metrics` in `ClientOptions` and the usage line.
- Tests: `tests/client_session_test.cpp`, the option off by default and read with the
  others.

## Changed
- Documentation: F12 and `--metrics` in *Play* (options, controls, and a *Frame metrics*
  section with a screenshot, `docs/images/frame-metrics.png`); `client_app` in the code map
  of the architecture page.
- Changelog: #82 marked as merged (`81877d8`).

## Commits
- `8897168` feat(client): showed the frame metrics with f12 or --metrics
- `1e0ab36` feat(tests): added the --metrics option to the client options tests
- `4a7da6d` feat(docs): documented the frame metrics and updated changelogs
- Merge of `develop` (#82's text fields), and #82 marked as merged

## Notes
- Built and run on macOS (AppleClang 21, warnings as errors) against the engine's
  `feat/tools-frame-metrics` through `FETCHCONTENT_SOURCE_DIR_AEGIS2D`: a server and a
  client with `--join --metrics` show 119 frames per second, 8.4 ms a frame, 11 entities,
  410 sprites in 12 draw calls (the screenshot of the docs); 255 of the 257 ctest tests
  pass, all but the two flood tests, which bind 127.0.0.2 that macOS does not route and fail
  the same way on `develop`; `tools/format.sh`; `tools/lint.sh` clean on the changed files;
  `mkdocs build --strict`.
- The CI of this pull request builds against the engine's `develop`, so it fails until
  Aegis2D-Dev/aegis2d#460 is merged; the pull request is a draft until then.
