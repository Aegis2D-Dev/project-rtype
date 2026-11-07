# Level editor timeline: the waves moved in time

| | |
|---|---|
| Pull request | [#89](https://github.com/Aegis2D-Dev/project-rtype/pull/89) (opened 2026-11-06) |
| Branch | `feat/client-editor-timeline` → `feat/client-level-editor` (stacked on #88) |
| Status | Open |
| Author | @Neptnium |
| Ticket | [Aegis2D-Dev/aegis2d#370](https://github.com/Aegis2D-Dev/aegis2d/issues/370) feat(tools): level editor waves timeline |

## Summary
The level editor (#88) shows a timeline of the waves under the play area: a block per wave
from the end of its delay to its last enemy, as the server plays them. A wave taken on the
timeline follows the mouse, which changes its delay and moves the waves after it; Left and
Right change the delay by a tenth of a second, Up and Down the order of the waves. Stacked
on #88.

## Added
- `LevelEditor::timeline()`: a `WaveSpan` (start, end) per wave, a wave starting its delay
  after the last enemy of the one before, as `ecs::WaveSchedule` plays them.
- `LevelEditor::shift_delay(seconds)`: the delay of the wave edited, at least 0, rounded to
  a tenth of a second; `move_wave(later)`: the wave edited swapped with its neighbour, and
  still edited; `dragging_wave()`.
- `LevelEditor::update()` on the timeline: a click edits the wave started last before the
  time clicked, which follows the mouse while the button is held; no enemy is placed there.
- `EditorView::timeline`, `timeline_size`, `timeline_seconds()` (at least 10 s, a fifth more
  than the last wave) and `on_timeline()`; the play area shrunk to 78 % to leave it room.
- `client_app`: Left and Right, Up and Down in the editor; `editor_view.cpp` draws the
  timeline, a mark every 5 seconds, the wave edited in yellow, and its delay, start and end.
- Tests: `tests/level_editor_test.cpp` (3 test cases): the timeline of two waves, a wave
  moved in time with the mouse (the ticket's bar), and the keys on the delay and the order.

## Changed
- Documentation: the timeline and its keys in the *Level editor* section of *Play*.

## Commits
- `eae72ca` feat(client): added the waves timeline to the level editor
- `ad1b2e9` feat(tests): added the waves timeline tests
- feat(docs): documented the waves timeline and updated changelogs

## Notes
- Built and tested in the CI image (clang 18, warnings as errors) against the engine's
  `develop` (`ad3c48c`): 277 of the 277 ctest tests pass; `tools/lint.sh` clean on the
  changed files; `tools/format.sh --check`; `mkdocs build --strict`. Built on macOS
  (AppleClang 21).
