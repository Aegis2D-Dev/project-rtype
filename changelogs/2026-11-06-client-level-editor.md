# Level editor: the enemies of a wave placed with the mouse

| | |
|---|---|
| Pull request | [#88](https://github.com/Aegis2D-Dev/project-rtype/pull/88) (opened 2026-11-06) |
| Branch | `feat/client-level-editor` → `develop` |
| Status | Open |
| Author | @Neptnium |
| Ticket | [Aegis2D-Dev/aegis2d#369](https://github.com/Aegis2D-Dev/aegis2d/issues/369) feat(tools): level editor mode |

## Summary
`r-type_client --edit <waves file>` opens the level editor: the game stands still with no
server, the waves of the file are shown one at a time, and the mouse places, moves and
removes their enemies. The editor's logic lives in the client's library without a window,
so the tests drive it as the mouse would; the next tickets edit the timing of the waves
(Aegis2D-Dev/aegis2d#370) and save and play them (Aegis2D-Dev/aegis2d#371).

## Added
- `rtype/level_editor.hpp` (`client/network`):
  - `LevelDraft::from_file()`: a waves file, checked as the server reads it
    (`load_waves()`), as `DraftWave`s (their delay) of `DraftEnemy`s (type, position, the
    second they appear in their wave), each group of the file one enemy per member;
  - `EditorView`: where the play area is drawn, shrunk to 85 % to show the strip of 96
    pixels beyond its right edge, where the enemies appear (`to_screen()`, `to_level()`);
  - `LevelEditor`: the wave edited (`next_wave()`, `previous_wave()`, looping), and
    `update(pointer, view)` with the mouse of a frame (`EditorPointer`): a left click on an
    empty place puts a Bydos slave there, on an enemy takes it, and either follows the
    mouse while the button is held, keeping where it was taken; a right click removes the
    enemy under the mouse; positions in whole pixels, kept within the play area and the
    strip; `hovered()`, `dragged()`, `changed()`.
- `--edit <file>` in `ClientOptions` and the usage line; `ClientScreen::Editor` in
  `client_app` (Page Up and Page Down change the wave, Escape leaves it), drawn by
  `editor_view.cpp`: the play area and the strip, the enemies with the Bydos sprite (the
  one under the mouse in yellow) and their second, a cross on the mouse, the wave and the
  count of enemies at the top, the controls at the bottom; a file that does not load is
  shown as an error on the connect screen.
- Tests: `tests/level_editor_test.cpp` (5 test cases): `stage1.json` opened as its 29
  enemies, broken and missing files refused, the view, enemies placed (the ticket's bar),
  moved and removed with the mouse, and the waves gone through;
  `tests/client_session_test.cpp`, `--edit` read.

## Changed
- Documentation: the level editor and `--edit` in *Play*.
- Changelog: #83 marked as merged (`4c7e3f1`).

## Commits
- `8d45117` feat(client): added the level editor, placing the enemies of a wave with the mouse
- `ed23ac9` feat(tests): added the level editor tests
- feat(docs): documented the level editor and updated changelogs

## Notes
- Built and tested in the CI image (clang 18, warnings as errors) against the engine's
  `develop` (`ad3c48c`): 274 of the 274 ctest tests pass; `tools/lint.sh` clean on the
  changed files; `tools/format.sh --check`; `mkdocs build --strict`. Built on macOS
  (AppleClang 21), where `--edit` opens the editor; its window does not draw while hidden
  behind others on this machine, so no screenshot was taken.
- "Pause": the editor is a screen of its own with no server, where nothing moves; editing
  the waves of a game being played is out of its reach (the server owns the game).
