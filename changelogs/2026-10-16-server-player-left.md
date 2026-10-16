# Players told who joins and who leaves

| | |
|---|---|
| Pull request | [#34](https://github.com/Aegis2D-Dev/project-rtype/pull/34) (opened 2026-10-16) |
| Branch | `feat/server-player-left` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#118 notify players when a client disconnects |

## Summary
Until now a player who left only showed as its ship disappearing. The server now sends the
reliable events of RFC section 4.6: when a player leaves, the others get its `PlayerLeft`
with the reason (`Left` for a `Logout`, a `Disconnect` or a closed connection, `TimedOut`
for a silent player, `Kicked` for a protocol error), then the `Destroy` of its ship. When a
player joins, the others get its `PlayerJoined` with its name, and it gets a
`PlayerJoined` of every player already there before the world. Part of the game server
epic (Aegis2D-Dev/aegis2d#107).

## Added
- `rtype/events.hpp`: `PlayerJoinedEvent` (number and name), `PlayerLeftEvent` (number and
  `LeaveReason`), written and read as RFC section 4.4 says; a name that breaks the rules
  of `LoginRequest` or a player above 3 is rejected, an unknown reason kept.
- `ServerNetwork::LeaveHandler`: the handler of a player who leaves gets the reason;
  `send_events_to_others()` sends to every bound player but one.
- `GameServer`: keeps the names of the players in the game (simulation thread); sends
  `PlayerJoined` and `PlayerLeft` as above, a `PlayerLeft` always before the next tick's
  `Destroy` of the ship.
- Tests: the two events byte by byte, read back, malformed ones; the reason of each way of
  leaving (`Logout`, a second `LoginRequest`, `Disconnect`, closed connection, UDP
  timeout); end to end with `GameServer`, who learns which names, the newcomer never its
  own, and a kicked player's `PlayerLeft` before the `Destroy` of its ship.

## Changed
- `ServerNetwork::on_players()` takes a `LeaveHandler`; the tests' handlers follow.
- Architecture page and *Getting started*: who is told what when a player joins or leaves.
- Changelog: #33 (`49cff6a`) marked as merged, with its commits.

## Commits
- `d3d4a20` feat(server): told players who joins and who leaves
- `99af4c5` feat(docs): documented player notices and updated changelogs
- feat(changelog): updated changelogs

## Notes
- The ticket's "remaining clients display the notice" needs the client's networking (#124
  to #128, and the notice of #129); this PR does the server side and checks it over real
  sockets.
- A player who acknowledges no event (RFC section 6.2) leaves as `TimedOut`.
- `GameOver` is still not sent: it comes with the end of a game.
- Verified on Linux (NixOS, Clang 20) against the engine's `develop`: 103 tests, also
  under ASan + UBSan and ThreadSanitizer, the new server ones 20 times in a row (5 under
  each sanitizer); clang-format and clang-tidy 22 on the touched files.
