# Chat relayed in each room, with length and rate limits

| | |
|---|---|
| Pull request | [#93](https://github.com/Aegis2D-Dev/project-rtype/pull/93) (opened 2026-11-07) |
| Branch | `feat/server-chat-relay` → `develop` |
| Status | Merged on 2026-11-07 (`27b5d25`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#308](https://github.com/Aegis2D-Dev/aegis2d/issues/308) feat(server): text chat relay |

## Summary
Players chat over TCP, in the lobby, in a room and in game, as section 3.8 of the RFC now says
(Aegis2D-Dev/aegis2d#476): the server relays a message to the players of the sender's room
only, or of the lobby, the sender included. A text longer than 200 bytes, empty or with
control characters is refused with its reason, and a flood is cut: a session sends 5 messages
at once, then 1 more per second, the others refused and not relayed; the connection goes on.
Tenth ticket of the *Network: multi-instance server* epic (Aegis2D-Dev/aegis2d#298); the
client shows the chat with #309.

## Added
- `rtype/chat.hpp`: `ChatSend`, `ChatMessage` and `ChatRejected` (`TooLong`, `BadText`,
  `TooFast`), read and written; `check_chat_text()`, `max_chat_size` (200) and
  `lobby_player` (255); `TcpKind` 17 to 19.
- `valid_text()`, the UTF-8 rules of player names for any length; `valid_player_name()` uses
  it.
- `ChatRate`, the token bucket of a session: 5 messages at once, then 1 back per second.
- `ServerNetwork` relays each accepted `ChatSend` to the sessions of the sender's room, or of
  the lobby, and answers a refused one with `ChatRejected`, logged at debug level only.
- Tests: the bytes of the RFC example, malformed messages, the text rules, the rate over time;
  a chat that stays in its room and in the lobby, a long message and a bad text refused, and a
  flood of 12 messages of which only those within the rate reach the room.

## Changed
- `kind_of()` reads kinds 17 to 19; 20 to 255 are reserved.
- `ClientSession` ignores `ChatMessage` and `ChatRejected` instead of ending the session, until
  the client shows the chat (#309).
- *Architecture*: the chat.
- Changelog, with `develop` merged in: #80 (`ce5d219`) and #91 (`7d37054`) marked as merged.

## Commits
- `62869c6` feat(server): relayed the chat of each room with length and rate limits
- `d58e044` feat(server): counted the chat refill from the last full bucket
- `5bf657d` feat(tests): added a chat flood cut and a long chat message refused
- `348c148` feat(docs): documented the chat relay and updated changelogs
- Merge remote-tracking branch 'origin/develop' into feat/server-chat-relay

## Notes
- The RFC part is Aegis2D-Dev/aegis2d#476, merged on 2026-11-07. `develop` was merged in
  for its new commits (enemy scripts, level editor, server trace): the only conflict was the
  index of `CHANGELOG.md`.
- The rate is per session, on top of the TCP limits of an address; a refused message does not
  count against it.
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 (73 files), `mkdocs build --strict`, Doxygen.
- `ctest`: 292 of 293 tests pass; under ASan and UBSan, the test cases of the chat, the logins,
  the rooms and the client pass. *a flooding address is cut off, and the players play on* fails
  on the machine used, as on `develop`, and passes in CI.
