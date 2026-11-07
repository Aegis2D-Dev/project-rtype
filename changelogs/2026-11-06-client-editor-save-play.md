# Level editor save and play: new waves without editing JSON

| | |
|---|---|
| Pull request | [#90](https://github.com/Aegis2D-Dev/project-rtype/pull/90) (opened 2026-11-06) |
| Branch | `feat/client-editor-save-play` → `feat/client-editor-timeline` (stacked on #89) |
| Status | Merged on 2026-11-07 (`c16bd94`) |
| Author | @Neptnium |
| Ticket | [Aegis2D-Dev/aegis2d#371](https://github.com/Aegis2D-Dev/aegis2d/issues/371) feat(tools): level editor save and play |

## Summary
The level editor (#88, #89) saves its waves to the waves file, and plays them from any wave
on the client: N adds a wave, the mouse places its enemies, S saves the file once the
server would read it, and P plays from the wave shown, with the player's ship and the
server's game ticked on the client. A new wave is made and played with no JSON written by
hand. Stacked on #89.

## Added
- `LevelDraft::to_json(first_wave)`: the waves as a waves file, each enemy a spawn of its
  own (`at` written when not 0), from a wave on; `save(path)`: written next to the file and
  checked as the server reads it (`load_waves()`) before it replaces it, so a draft the
  server would refuse (a wave with no enemy...) leaves the file as it was and says why;
  `schedule(first_wave)`: the `ecs::WaveSchedule` the server would play from a wave.
- `LevelEditor::add_wave()`: an empty wave after the one edited, 2 seconds after it, edited;
  `mark_saved()`: `changed()` is `false` again once saved.
- `client_app`: N, S and P in the editor, the message of a save (or why not) above the
  controls; `ClientScreen::Playtest`, a `ServerGame` of the waves from the wave edited with
  the player's ship, ticked 60 times a second on the client with the keys of the game;
  Escape comes back to the editor; drawn by `draw_playtest()` (the ship and the enemies with
  their sprites, the missiles, the score and lives, game over).
- Tests: `tests/level_editor_test.cpp` (4 test cases): a draft saved and read back as the
  server reads it, from a wave on; a draft the server would refuse leaving the file as it
  was; a new wave added to a copy of `stage1.json`, its enemies placed with the mouse,
  saved, then played from that wave, its enemies appearing where they were placed (the
  ticket's bar); a wave added after the one edited.

## Changed
- The editor's play area is shrunk to 74 %, to leave room for two lines of controls and its
  messages.
- Documentation: N, S and P, the save and the playtest, in the *Level editor* section of
  *Play*.

## Commits
- `ce8476b` feat(client): saved the level editor's waves and played them from any wave
- `839ebbe` feat(tests): added the level editor save and play tests
- feat(docs): documented the level editor save and play and updated changelogs

## Notes
- Built and tested in the CI image (clang 18, warnings as errors) against the engine's
  `develop` (`ad3c48c`): 281 of the 281 ctest tests pass; `tools/lint.sh` clean on the
  changed files; `tools/format.sh --check`; `mkdocs build --strict`. Built on macOS
  (AppleClang 21).
- The playtest plays the waves on the client; the server plays a saved file with
  `r-type_server --waves <file>`.
