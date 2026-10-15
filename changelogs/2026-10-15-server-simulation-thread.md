# 60 Hz simulation thread

| | |
|---|---|
| Pull request | [#28](https://github.com/Aegis2D-Dev/project-rtype/pull/28) (opened 2026-10-15) |
| Branch | `feat/server-simulation-thread` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#112 60 Hz simulation thread |

## Summary
The server's game runs on its own thread at 60 ticks per second, with or without players.
The network thread no longer touches the game: it posts the changes to make (players
joining and leaving, and the inputs of #113) to the simulation thread. Part of the game
server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `rtype::Simulation` (`server/include/rtype/simulation.hpp`): runs a `GameInstance` on a
  thread at `tuning::tick_rate` with the engine's `MainLoop`, which sleeps until each step
  is due and catches up at most 250 ms of steps after a stall. `post()` queues a command
  that runs on the thread, in order, before the next tick. A command or a tick that throws
  is logged and the thread goes on. `stop()` returns at once, even before the first tick.
  The measured rate is logged every 10 seconds.
- Tests (`tests/simulation_test.cpp`): the rate with no player, commands from another
  thread, and stopping.

## Changed
- `r-type_server` starts the simulation once its ports are bound and posts the joins and
  leaves to it; its start line shows the rate.
- `rtype_server` links `Threads::Threads`.
- *Getting started*: what the server does now.
- Changelog: #27 (`38f0818`) marked as merged, with its commits and the fix after its
  first CI run.

## Commits
- `0b752e0` feat(server): ran the game on a 60 hz simulation thread

## Notes
- The way back, from the simulation thread to the network (snapshots and events), comes
  with #114 and #115.
- Verified: 77 tests, the simulation ones 10 times in a row (60 ticks in one second, 59.99
  Hz), the server tests under ThreadSanitizer, the suite under ASan and UBSan, clang-format
  and clang-tidy 22, Doxygen, `mkdocs build --strict`, and `r-type_server` logging
  `600 ticks in 10.0 s (60.0 Hz)` with no client.
