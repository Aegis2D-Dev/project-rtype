# Lag compensation for hits

| | |
|---|---|
| Pull request | [#72](https://github.com/Aegis2D-Dev/project-rtype/pull/72) (opened 2026-11-04) |
| Branch | `feat/server-lag-compensation` → `develop` (stacked on `feat/server-position-history`, #71) |
| Status | Merged on 2026-11-04 (`b168095`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#297](https://github.com/Aegis2D-Dev/aegis2d/issues/297) feat(server): lag compensation for hits |

## Summary
A player's missiles are also checked against the enemies where that player saw them: rewound
by its round trip time and its client's interpolation delay, within the 500 ms of the
position history (#71). At 200 ms of round trip, a missile where the player saw an enemy, 72
pixels from where the enemy is now, kills it on the server; without the rewind it misses.
Eighth ticket of the *Network: responsiveness under lag* epic (Aegis2D-Dev/aegis2d#289).

## Added
- `rtype/lag_compensation.hpp`, in `rtype_game`: `rewind_ticks(rtt, interpolation)`, their
  sum in ticks, rounded, at most `PositionHistory::capacity` (30); `find_rewound_contacts()`,
  the contacts of each player missile, where it is now, with each enemy still alive where the
  history had it its shooter's rewind ago, written as `ecs::find_contacts()` writes them.
- `ServerGame::set_rewind(player, ticks)` and `rewind(player)`, per slot, back to 0 when the
  slot gets a new ship; `GameInstance::set_rewind(player, ticks)`.
- `tuning::client_interpolation_delay`: 100 ms, the client's first interpolation delay, as
  the server assumes it.
- Tests: 200 ms and the 100 ms of interpolation are 18 ticks, 1 s is held to 30. An enemy
  going down at 240 px/s: a still missile where the player saw it 18 ticks ago kills it and
  is consumed; the same shot without the rewind misses. A rewind of 60 is held to 30 and a
  shot 40 ticks back misses; an enemy destroyed since is not hit; a player out has no rewind,
  nor a new ship in its slot. Against a real `GameServer`: once the round trip is measured,
  the game's rewind of the player is the one of its smoothed round trip time.

## Changed
- `ServerGame`: the `collision` system adds the rewound contacts to the others before
  `damage`; a missile that hits both now and then is consumed by the first.
- `GameServer`: with each snapshot, on the network thread, each player's smoothed round trip
  time (`ServerNetwork::rtt()`) becomes a rewind, posted to the game when it changes.
- *Gameplay*: the `collision` system, the new calls, and how the rewind is chosen.
- Changelog: with `develop` merged in, #70 (`6c9e6fe`) marked as merged.

## Commits
- `2e4df4d` feat(server): checked missiles against the enemies their shooter saw
- `2ef0676` feat(tests): added lag compensation tests at 200 ms of round trip
- `11b15ce` feat(docs): documented lag compensation and updated changelogs
- `2b9630c` Merge remote-tracking branch 'origin/develop' into feat/server-lag-compensation
- feat(changelog): fixed pr numbers and merge status

## Notes
- Stacked on #71, merged since (`1801efc`); now against `develop`, which was merged in once
  #64 to #71 were there: the conflicts were additions on both sides (the paragraphs of
  *Gameplay* on the ship step and on lag compensation, the test list, the includes of the
  client tests, the index of `CHANGELOG.md`). Built again (server and client), clang-tidy
  on the merged sources: 237 of 238 tests pass.
- To arbitrate: the server assumes the client's first interpolation delay, 100 ms. With the
  adaptive delay of #70 (50 to 200 ms), the client should report its delay (in its `Input`,
  an RFC change) for the rewind to follow it; not done here, so as not to change the protocol
  again on top of #64 and #65.
- A missile is compensated against enemies only, not against enemy missiles; enemies'
  missiles are not compensated against players (the player sees its own ship in the present,
  #67).
- Verified with the engine's `develop` (`server` preset, Clang 20, warnings as errors), the
  client built too: clang-format, clang-tidy 22 on every changed source, `mkdocs build
  --strict`, Doxygen; the lag compensation and history tests also under ASan and UBSan.
- `ctest`: 211 of 212 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
