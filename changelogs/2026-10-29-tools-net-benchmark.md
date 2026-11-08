# Network benchmark runs

| | |
|---|---|
| Pull request | [#55](https://github.com/Aegis2D-Dev/project-rtype/pull/55) (opened 2026-10-29) |
| Branch | `feat/tools-net-benchmark` → `develop` (stacked on `feat/tools-bot-scripts`, #54, merged) |
| Status | Merged on 2026-10-29 (`8db2fb7`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#275](https://github.com/Aegis2D-Dev/aegis2d/issues/275) network benchmark runs |

## Summary
`tools/net_benchmark.py run` measures the game's network under four netem profiles (LAN,
50 ms, 150 ms + 2 % loss, 300 ms + 5 % loss) in one command: a server, four bots for 30
seconds per profile, and one CSV per machine with the bandwidth, the round trip time, the
loss and the jitter the bots saw. The numbers the optimizations of the *data transmission*
and *responsiveness* epics will be compared with. Seventh ticket of the *Network:
measurements and lag tools* epic (Aegis2D-Dev/aegis2d#268).

## Added
- `tools/net_benchmark.py` (standard library only, like the engine's `tools/benchmark.py`):
  for each profile, starts `r-type_server` (its output in a temporary log), degrades its
  two ports on `lo` with the engine's `tools/netem.sh` (found from the build tree's
  `aegis2d_SOURCE_DIR`, with `sudo` unless root), runs `r-type_bot --count 4 --seconds 30
  --seed 1 --stats`, removes the rules and stops the server even after an error, and
  writes the means of the bots to `docs/network-benchmark/<machine>.csv`. Options:
  `--profiles`, `--bots`, `--seconds`, `--netem`, `--dev`, the ports, `--output`, and
  `--dry-run`, which prints the `tc` commands and writes nothing unless `--output` is
  given.
- `rtype/bot_stats.hpp`: `measure_bot()` and `bot_stats_line()`; `r-type_bot --stats
  <file>` writes one CSV line per bot that played, measured when it leaves: time in game,
  inputs, snapshots, bytes in and out per second, RTT and its jitter, loss and snapshot
  jitter over the last 2 seconds.
- Tests: the CSV line and its columns, the `--stats` option, a bot that never played, and
  the figures of a bot that played a real server.
- *Getting started*: `--stats` and its columns, and a *Network benchmark* section with the
  command, the profiles, root and `sudo`, and the columns of the CSV. *Architecture*: the
  script in the code map.

## Changed
- `r-type_bot`: what it measured is kept when each bot leaves, by `--seconds` or Ctrl+C,
  for `--stats`; its usage lists the option.
- Changelog: #53 merged on 2026-10-29 (`024b3a8`) and #54 on 2026-10-29 (`49f9c0e`), with
  their last commit.

## Commits
- `2997770` feat(tools): added the network benchmark script and r-type_bot --stats
- `8d3bd66` feat(tests): added bot stats tests
- `4eddf89` feat(docs): documented the network benchmark and updated changelogs
- `c0b908d` feat(changelog): updated changelogs

## Notes
- The `corrections` column stays empty: the client corrects nothing yet; client-side
  prediction and its correction count (Aegis2D-Dev/aegis2d#292 to #294) will fill it.
- netem delays each packet, so on `lo` each direction: the round trip grows by twice the
  profile's delay. The profiles are those of the ticket, with neither duplication nor a
  bandwidth cap (`--duplicate 0% --rate 1gbit`), unlike `netem.sh`'s defaults.
- Verified on the machine used, without root: the `lan` profile for real (4 bots, about
  11 KB/s in and 1.1 KB/s out each, RTT 12 ms, no loss), and every profile with
  `--dry-run` (the `tc` commands of each, the rules removed after each). **The degraded
  profiles still need one real run with root** (`sudo` asks for the password); no CSV is
  committed, the report (Aegis2D-Dev/aegis2d#276) will publish the first ones.
- Verified: 164 tests (`server` configuration against the engine's `develop`, the new ones
  5 times in a row), warnings as errors, clang-format and clang-tidy 22, `mkdocs build
  --strict`, Doxygen without warnings.
- `a flooding address is cut off, and the players play on` fails on the machine used, with
  builds of 2026-10-17 too (environment, see #52); it passes in CI.
