# Best scores in the lobby

| | |
|---|---|
| Pull request | [#96](https://github.com/Aegis2D-Dev/project-rtype/pull/96) (opened 2026-11-07) |
| Branch | `feat/client-lobby-scoreboard` → `develop` (stacked on `feat/server-scoreboard`, #95) |
| Status | Merged on 2026-11-07 into `feat/server-scoreboard` (`c7033b0`), brought to `develop` by #97 |
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
- `kind_of()` reads kinds 1 to 21, those of the chat (#93) and of the scoreboard together.
- *Playing*: the best scores of the rooms screen. *Architecture*: the scores read by the
  lobby.

## Commits
- `5a971a8` feat(client): showed the best scores of the server in the lobby
- `7b217f9` feat(tests): added the best scores shown in the lobby
- `38ec870` feat(docs): documented the best scores of the lobby and updated changelogs
- Merge branch 'feat/server-scoreboard' into feat/client-lobby-scoreboard
- `1f8f04e` feat(changelog): updated changelogs
- `0737ce6` feat(changelog): noted the windows failure of the draft
- Merge branch 'feat/server-scoreboard' into feat/client-lobby-scoreboard

## Notes
- Stacked on #95, which keeps the board; #95 was merged in once it had `develop`, without
  conflict. The RFC part is Aegis2D-Dev/aegis2d#478: the
  scoreboard takes kinds 20 and 21 after the chat's 17 to 19 (Aegis2D-Dev/aegis2d#476). #95 was
  merged in again once it had #92 to #94: `TcpKind` keeps the kinds of the chat and of the
  scoreboard, now contiguous, and `ClientSession` both the chat and the scores.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too (`-DSDL_UNIX_CONSOLE_BUILD=ON`): clang-format, clang-tidy 22 (75 files, and
  86 with the client), `mkdocs build --strict`, Doxygen. The screen was not checked on a
  window: the machine used has no display.
- `ctest`: 309 of 310 tests pass; under ASan and UBSan, the test cases of the scores, the
  client and the logins pass. *a flooding address is cut off, and the players play on* fails
  on the machine used, as on `develop`, and passes in CI.
- The *Build and test (Windows)* job failed on *the client's entities mirror the server's*
  (SEGFAULT), a test this pull request does not change: the same crash hit #93, which has none
  of its code, since `develop` got the enemy scripts and the level editor; a rerun passed.

