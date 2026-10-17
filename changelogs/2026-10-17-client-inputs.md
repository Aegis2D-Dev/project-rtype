# Client inputs sent every tick

| | |
|---|---|
| Pull request | [#40](https://github.com/Aegis2D-Dev/project-rtype/pull/40) (opened 2026-10-17) |
| Branch | `feat/client-inputs` → `develop` |
| Status | Merged on 2026-10-17 (`d08f5ec`) |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#125 send input state every tick |

## Summary
Once in the game, the client sends the actions its player holds in one `Input` per server
tick, 60 per second (RFC section 4.2), each acknowledging the server's datagrams. The
server receives them at 59.97 Hz. Part of the client networking epic
(Aegis2D-Dev/aegis2d#122).

## Added
- `ClientSession::set_input()`: the held actions, sent in every `Input` until the next
  call; bits 5 to 7 cleared.
- `ClientSession::input_interval` (one tick, from `tuning::tick_rate`) and
  `inputs_sent()`.
- Tests: the 60 Hz rate measured by the server, the acknowledgements carried by the inputs,
  and a ship moving in the server's game.

## Changed
- `ClientSession::update()`: in game, an `Input` every interval on a schedule that does
  not drift, without a burst after a stall.
- `rtype_client_net` depends on `rtype_game`.
- Changelog: #39 (`9f0b0ff`) marked as merged, with its commits.
- After the first CI run: the rate test compares the inputs the server received with those
  sent during the measured second only, as the last ones sent after it may still be on
  their way.

## Commits
- `ec7af89` feat(client): sent the held actions every tick
- `b0a6149` feat(changelog): updated changelogs
- `e3c8511` feat(tests): counted only the inputs of the measured second

## Notes
- The game loop will call `set_input()`; the client has no window to read keys from yet.
- Verified: 125 tests, the client ones 10 times in a row, under ASan, UBSan and
  ThreadSanitizer, clang-format and clang-tidy 22, Doxygen; the schedule sends exactly 60
  inputs per second when driven with synthetic times.
