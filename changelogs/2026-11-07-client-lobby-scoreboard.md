# Best scores in the lobby

| | |
|---|---|
| Pull request | [#96](https://github.com/Aegis2D-Dev/project-rtype/pull/96) (opened 2026-11-07) |
| Branch | `feat/client-lobby-scoreboard` → `develop` (stacked on `feat/server-scoreboard`, #95) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#313](https://github.com/Aegis2D-Dev/aegis2d/issues/313) feat(client): scoreboard in the lobby |

## Summary
The room list screen shows the best scores of the server under its rooms, as section 3.9 of
the RFC now says (Aegis2D-Dev/aegis2d#478): a client asks for them with `ListScores`, at the
login and at each refresh of the lobby, and the server answers with the 10 best scores of its
board (#95) in a `ScoreList`. Thirteenth ticket of the *Network: multi-instance server* epic
(Aegis2D-Dev/aegis2d#298).

## Added
- `rtype/scores.hpp`: `ListScores` and `ScoreList` (`ScoreRecord`, `max_listed_scores`),
  read and written; `TcpKind` 20 and 21.
- `ServerNetwork::set_scoreboard()`: a `ListScores` gets the board, or no score without one;
  `GameServer::keep_scores()` gives it its board.
- `ClientSession::scores()`, asked with the rooms; a score with a bad name is left out.
- The *BEST SCORES* of the room list screen, in two columns of 5 (`draw_rooms()`).
- Tests: the bytes of the RFC example, malformed lists and a list of 12 written as 10; a client
  in the lobby getting the scores of the server's board.

## Changed
- `kind_of()` reads kinds 20 and 21; 17 to 19 stay reserved here until the chat (#93) is
  merged.
- *Playing*: the best scores of the rooms screen. *Architecture*: the scores read by the
  lobby.

## Commits
- `5a971a8` feat(client): showed the best scores of the server in the lobby
- `7b217f9` feat(tests): added the best scores shown in the lobby
- `38ec870` feat(docs): documented the best scores of the lobby and updated changelogs
- Merge branch 'feat/server-scoreboard' into feat/client-lobby-scoreboard
- `1f8f04e` feat(changelog): updated changelogs
- feat(changelog): noted the windows failure of the draft

## Notes
- Stacked on #95, which keeps the board; #95 was merged in once it had `develop`, without
  conflict. The RFC part is Aegis2D-Dev/aegis2d#478: the
  scoreboard takes kinds 20 and 21 after the chat's 17 to 19 (Aegis2D-Dev/aegis2d#476). Merging
  with #93 will conflict in `tcp_kind()` and `TcpKind`: both ranges are kept.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too (`-DSDL_UNIX_CONSOLE_BUILD=ON`): clang-format, clang-tidy 22 (75 files, and
  86 with the client), `mkdocs build --strict`, Doxygen. The screen was not checked on a
  window: the machine used has no display.
- `ctest`: 297 of 298 tests pass; under ASan and UBSan, the test cases of the scores, the
  client and the logins pass. *a flooding address is cut off, and the players play on* fails
  on the machine used, as on `develop`, and passes in CI.
- **Draft**: the *Build and test (Windows)* job of CI fails twice in a row on *the client's
  entities mirror the server's* (SEGFAULT), a test this pull request does not change; the same
  test crashed once on #95 since `develop` was merged in, and passed on its rerun. It passes
  30 times out of 30 on the machine used, 10 out of 10 under ASan and UBSan, and under TSan; no
  code of this pull request runs in it (no game ends there, no scores are asked). It needs a
  Windows machine to debug before review.

