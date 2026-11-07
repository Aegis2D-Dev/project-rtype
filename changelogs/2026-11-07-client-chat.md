# Chat in the client, opened with T

| | |
|---|---|
| Pull request | [#94](https://github.com/Aegis2D-Dev/project-rtype/pull/94) (opened 2026-11-07) |
| Branch | `feat/client-chat` → `develop` (stacked on `feat/server-chat-relay`, #93) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#309](https://github.com/Aegis2D-Dev/aegis2d/issues/309) feat(client): chat |

## Summary
`r-type_client` chats: T opens a text line at the bottom left on the room list, in a room and in
game, Enter sends it to the players of the room (or of the lobby) and Escape closes it. The
last 6 messages show above the line for 8 seconds, each in its sender's color, and a message
the server refuses says why there. Two clients chat in the lobby and in game. Eleventh ticket
of the *Network: multi-instance server* epic (Aegis2D-Dev/aegis2d#298), on the relay of #93.

## Added
- `ChatLog`: the last messages, `"name: text"` in the sender's slot (`lobby_player` for the
  lobby and the refusals), 6 at most for 8 seconds; a message breaking the rules of the
  protocol is not shown. `chat_rejection_text()`.
- `ClientSession::send_chat()`, once logged in, and `chat()`, filled by the `ChatMessage` and
  `ChatRejected` of the server.
- The chat line of `r-type_client`: an engine `TextField` on the platform's text input, its
  messages drawn by `draw_chat()` over the room list, the room and the game; the ship stops
  while its player types. `chat_length` (50 characters) always fits the server's 200 bytes.
- Tests: the lines of the log, their limit, their time and the refusals; two clients chatting
  in the lobby, then in game, and a refused message told to its sender only.

## Changed
- `ClientSession` shows the chat instead of ignoring it (#93).
- *Playing*: the chat key. *Architecture*: the chat of the client.

## Commits
- `6ff95ad` feat(client): added the chat line and the last messages of the room
- `0b9bd40` feat(tests): added two clients chatting in the lobby and in game
- `5e92099` feat(docs): documented the chat of the client and updated changelogs
- Merge branch 'feat/server-chat-relay' into feat/client-chat

## Notes
- Stacked on #93, which relays the chat; its RFC part is Aegis2D-Dev/aegis2d#476, merged.
  #93 was merged in once it had `develop` (level editor): the only conflicts were the screens
  of `ClientApp`, the chat line keeping the keys of the editor screens too while it is open.
- On the room list, T is a letter of the new room's name while one is typed: the chat opens
  from the other rows.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too (`-DSDL_UNIX_CONSOLE_BUILD=ON`): clang-format, clang-tidy 22 (75 files, and
  93 with the client), `mkdocs build --strict`, Doxygen. The drawing was not checked on a
  window: the machine used has no display.
- `ctest`: 294 of 295 tests pass; under ASan and UBSan, the test cases of the chat and the
  client pass. *a flooding address is cut off, and the players play on* fails on the machine
  used, as on `develop`, and passes in CI.
