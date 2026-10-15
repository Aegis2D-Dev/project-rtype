# Game instance with up to 4 players

| | |
|---|---|
| Pull request | [#27](https://github.com/Aegis2D-Dev/project-rtype/pull/27) (opened 2026-10-15) |
| Branch | `feat/server-game-instance` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#111 game instance with up to 4 players |

## Summary
The server has a game its players join and leave: a player joins with the ship of its
session's player number once its UDP endpoint is bound, and leaves when its session ends.
The 5th login gets `LoginRejected(ServerFull)`. When the last player leaves, a fresh game
waits for the next ones. The simulation thread (#112) will run this game. Part of the game
server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `ServerGame::add_player(slot)`: the ship in a given slot, the player number, which picks
  its ship and spawn point; a slot in use or out of range is refused.
- `ServerGame::remove_player()`: a player who left loses its ship and frees its slot; the
  game goes on for the others.
- `rtype::GameInstance` (`server/include/rtype/game_instance.hpp`): the server's game, with
  `join()` and `leave()` by player number. A player out of lives keeps its number until it
  leaves; the last player leaving starts a fresh game with the same seed and waves.
- `ServerNetwork::on_players()`: called when a session joins (its endpoint bound) and when
  a session that joined ends.
- Tests: slots and removal in `ServerGame`, `GameInstance`, and over real sockets 4
  players joining as 0 to 3, the 5th refused, a player leaving, a login that never binds
  never joining.

## Changed
- `ServerGame::add_player()` goes through `add_player(slot)`.
- `r-type_server` creates the `GameInstance` with the waves and wires it to the sessions.
- *Getting started*: what the server does now.
- Changelog: #26 (`155871a`) marked as merged, with its commits.

## Commits
- `c9ef49c` feat(server): added the game instance with up to four players

## Notes
- The join and leave handlers run on the network thread: #112 moves the game to its own
  thread, behind a queue, and makes it tick.
- Verified: 74 tests, the server ones 20 times in a row, the suite under ASan and UBSan,
  clang-format and clang-tidy 22, Doxygen, `mkdocs build --strict`, and a Python client
  logging in and binding against `r-type_server`.
