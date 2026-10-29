# Headless bot client

| | |
|---|---|
| Pull request | [#53](https://github.com/Aegis2D-Dev/project-rtype/pull/53) (opened 2026-10-29) |
| Branch | `feat/tools-headless-bot` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#273](https://github.com/Aegis2D-Dev/aegis2d/issues/273) headless bot client |

## Summary
`r-type_bot` joins a server without a window and holds random inputs: a ClientSession
from `rtype_client_net`, nothing else, so every preset builds it, the `server` one
included. It logs in, binds its UDP endpoint, sends one `Input` per tick like a player
mashing the keys, and leaves with `Logout` after `--seconds` or at Ctrl+C. Fifth ticket of
the *Network: measurements and lag tools* epic (Aegis2D-Dev/aegis2d#268): the bots will
fill the games of the network benchmark.

## Added
- `tools/bot/` (`rtype_bot`, linked to `rtype_client_net` only):
  - `RandomInputs`: a direction or none on each axis, Fire 7 times in 10, each held 150 to
    900 ms; the same seed gives the same inputs at the same times;
  - `Bot`: a `ClientSession` fed by `RandomInputs` once in game; `start()`, `update()`,
    `leave()`, and the session for its state, error and traffic;
  - `parse_bot_options()`: `--host`, `--port`, `--name` (`bot`), `--seconds`, `--seed`,
    `--help`, `--version`.
- `r-type_bot`: prints its seed, the player it plays, and on leaving the inputs sent, the
  holds and the snapshots received; status 1 when it cannot join or the server goes
  away, 2 on a bad command line.
- Tests: the random inputs (only the five actions, never two opposite directions, every
  one of the 18 combinations, hold times, same seed same inputs); the command line; and a
  bot against a real `ServerNetwork`: it joins, the server applies several different
  inputs from it, and it leaves with the reason `Left`, without timeout nor error.
- *Getting started*: a *Bots* section with the options and the output. *Architecture*:
  `tools/bot/` in the code map. Doxygen reads `tools/bot/include`.

## Changed
- Changelog: #51 merged on 2026-10-28 (`bb02b79`), with its last commit.

## Commits
- `05dc6b5` feat(tools): added r-type_bot, a headless client holding random inputs
- `ae97b81` feat(tests): added bot tests joining, playing and leaving a real server
- feat(docs): documented the bot and updated changelogs

## Notes
- Ctrl+C sets a lock-free flag, as in `r-type_server`; the server's handler lives in
  `rtype_server`, which the bot does not link, so `main.cpp` has its own (with the same
  justified `NOLINT`).
- Not packaged with the server and the client (`aegis2d_install()`): a developer tool for
  now; the benchmark script (Aegis2D-Dev/aegis2d#275) runs it from the build tree.
- Verified: 159 tests (`server` configuration against the engine's `develop`, the new ones
  5 times in a row), warnings as errors, clang-format and clang-tidy 22, `mkdocs build
  --strict`, Doxygen without warnings. By hand: `r-type_server`, then `r-type_bot --seconds
  3` (joined, 181 inputs sent, 60 snapshots received, logged out) and a bot stopped with
  SIGINT (logged out on the server).
- `a flooding address is cut off, and the players play on` fails on the machine used, with
  builds of 2026-10-17 too (environment, see #52); it passes in CI.
