# Several bots and input scripts

| | |
|---|---|
| Pull request | [#54](https://github.com/Aegis2D-Dev/project-rtype/pull/54) (opened 2026-10-29) |
| Branch | `feat/tools-bot-scripts` → `develop` (stacked on `feat/tools-headless-bot`, #53, merged) |
| Status | Merged on 2026-10-29 (`49f9c0e`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#274](https://github.com/Aegis2D-Dev/aegis2d/issues/274) several bots and input scripts |

## Summary
`r-type_bot --count N` runs N bots in one process, named and seeded each their own way, and
`--inputs FILE` makes them replay a script of inputs instead of random ones: four bots
fill a game for a whole stage from one command, the same way at each run. Sixth ticket of
the *Network: measurements and lag tools* epic (Aegis2D-Dev/aegis2d#268), for the network
benchmark.

## Added
- `rtype/input_script.hpp`: `parse_input_script()` and `load_input_script()` read one step
  per line, `<milliseconds> <actions>` (`U`, `D`, `L`, `R`, `F`, or `-`), with `#`
  comments and blank lines, and name the faulty line; `ScriptedInputs` plays the steps on
  time from its first update and loops, skipping the steps a stall covered.
- `bot_team(base, count)`: one bot keeps its settings; several are named `<name>1`,
  `<name>2`... and seeded `seed + index`, sharing the script.
- `r-type_bot`: `--count` (1 to 64) and `--inputs`; one line per bot when it joins and
  when it leaves; a bot that cannot join or loses the server is reported and the others
  play on (exit status 1); an unreadable script exits with status 1.
- `tools/bot/dodge.txt`: an example script (forward, dodges while firing, back, rest).
- Tests: scripts (comments, tabs, CRLF, every error), their timing, loop and stalls; the
  names and seeds of a team; the new options; and five scripted bots against a real
  `ServerNetwork`: four fill the game as players 1 to 4, the fifth is told the server is
  full, the server applies the three steps of the script from each of the four, and all
  leave without timeout nor error.
- *Getting started*: `--count`, `--inputs`, the script format and the output.

## Changed
- `Bot` plays `RandomInputs` or `ScriptedInputs` (`BotSettings::script`); `Bot::picks()`
  replaces `Bot::inputs()`.
- Changelog: #53 merged on 2026-10-29 (`024b3a8`), with its last commit.

## Commits
- `808c28e` feat(tools): added --count and --inputs to r-type_bot
- `06895eb` feat(tests): added input script tests and four bots filling a game
- `fe26628` feat(docs): documented several bots and input scripts and updated changelogs
- `b3a174a` feat(changelog): updated changelogs

## Notes
- Done when, by hand: `r-type_server`, then `r-type_bot --count 4 --seconds 25` (the
  stage, `stage1.json`, lasts 19.3 s): the four bots joined as players 1 to 4, each sent
  1501 inputs and received 500 snapshots, the server saw 41 datagrams/s to each (snapshots
  and events), then they logged out. `--count 2 --inputs tools/bot/dodge.txt` played the
  script the same way.
- Verified: 163 tests (`server` configuration against the engine's `develop`, the new ones
  5 times in a row), warnings as errors, clang-format and clang-tidy 22, `mkdocs build
  --strict`, Doxygen without warnings.
- `a flooding address is cut off, and the players play on` fails on the machine used, with
  builds of 2026-10-17 too (environment, see #52); it passes in CI.
