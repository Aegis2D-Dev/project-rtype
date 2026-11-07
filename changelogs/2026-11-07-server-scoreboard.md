# Best scores kept in a JSON file across restarts

| | |
|---|---|
| Pull request | [#95](https://github.com/Aegis2D-Dev/project-rtype/pull/95) (opened 2026-11-07) |
| Branch | `feat/server-scoreboard` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#312](https://github.com/Aegis2D-Dev/aegis2d/issues/312) feat(server): scoreboard storage |

## Summary
`r-type_server` keeps the 10 best scores in a JSON file, `scores.json` by default: when the
game of a room is over, the score of each of its players goes on the board if it is among the
best. The file is written at each change through a temporary file that then replaces it, so a
crash never leaves half a file, and it is read back at the next start: the scores survive a
restart. A file that cannot be read or is not a board is logged and replaced by the next
score, and the server starts anyway. Twelfth ticket of the *Network: multi-instance server*
epic (Aegis2D-Dev/aegis2d#298); the lobby shows the board with #313.

## Added
- `Scoreboard` and `ScoreEntry`: the `max_entries` (10) best scores, best first, an equal
  score after the ones already there; `record()`, `best()`, `load_error()` and `file()`.
  A hand-edited file out of order or too long keeps its 10 best; an invalid name is never
  recorded.
- `GameServer::keep_scores()` and `scoreboard()`; `GameInstance::take_game_over()`, true
  once per game when it is over.
- `--scores <file.json>` on `r-type_server`, `ServerOptions::scores` and
  `default_scores_file`.
- Tests: the scores read back by a new board, with no temporary file left; the 10 best kept,
  low scores and bad names refused, a hand-edited file sorted and cut; five corrupted files
  reported, the board starting empty and the next score writing a valid file; a board whose
  folder cannot be created staying in memory; the option; a game over once per game; a game
  over on the server keeping its player's score in the file.

## Changed
- `r-type_server` prints the file of its scores at start.
- `.gitignore`: `scores.json` of a server run from the clone.
- *Getting started*: `--scores`. *Architecture*: the scoreboard.
- Changelog, with `develop` merged in: #80 (`ce5d219`), #91 (`7d37054`), #92 (`d274552`),
  #93 (`27b5d25`) and #94 (`846b3fb`) marked as merged.

## Commits
- `712d3f4` feat(server): kept the best scores in a json file across restarts
- `048b649` feat(tests): added the best scores kept across a restart
- `574552c` feat(docs): documented the scoreboard and updated changelogs
- `aeee14a` Merge remote-tracking branch 'origin/develop' into feat/server-scoreboard
- Merge remote-tracking branch 'origin/develop' into feat/server-scoreboard

## Notes
- `develop` was merged in once it had `--trace` (#87): the conflicts were the options of
  `r-type_server`, both kept, and the index of `CHANGELOG.md`. It was merged in again once
  #92 to #94 were there: `GameServer` keeps both the waves of the rooms and the scores, and
  *Architecture* both the chat and the scoreboard.
- The board is written on the network thread, at most once per player at the end of a game:
  a file of 10 scores.
- The atomic write is the engine's `core::Config::save()` (`<file>.tmp`, then a rename).
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors):
  clang-format, clang-tidy 22 (81 files), `mkdocs build --strict`, Doxygen.
- `ctest`: 306 of 307 tests pass; under ASan and UBSan, the test cases of the scores, the
  game instance and the options pass. *a flooding address is cut off, and the players play
  on* fails on the machine used, as on `develop`, and passes in CI.
