# Network benchmark report

| | |
|---|---|
| Pull request | [#56](https://github.com/Aegis2D-Dev/project-rtype/pull/56) (opened 2026-10-29) |
| Branch | `docs/net-benchmark-report` → `develop` (stacked on `feat/tools-net-benchmark`, #55, merged) |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#276](https://github.com/Aegis2D-Dev/aegis2d/issues/276) network benchmark report |

## Summary
`tools/net_benchmark.py report` publishes the network benchmark on a docs page, like the
engine's rendering benchmark: per machine, a table of every profile and every run, each
number with its change from the first run, and two bar charts, bandwidth and round trip
time. Runs carry a `--label` (`baseline`, then each optimization) and accumulate in the
machine's CSV, so the page shows the numbers before and after each optimization. Last
ticket of the *Network: measurements and lag tools* epic (Aegis2D-Dev/aegis2d#268).

## Added
- `report`: reads every CSV of `docs/network-benchmark/` and rewrites the results of
  `docs/network-benchmark.md` between its markers, with
  `docs/images/network-benchmark-<machine>-bandwidth.svg` and `-rtt.svg` (a group per
  profile, a bar per label, colors that stay apart with color blindness); running it
  twice gives the same page.
- *Network benchmark* page (in the nav): the method, the profiles, what each column
  means, how to measure a machine, and the results.
- The first results: the `lan` profile of a Ryzen 5 7640U under NixOS, `baseline`
  (13.9 KB/s in and 1.1 KB/s out per bot, RTT 13.5 ms, no loss).
- Tests (`tests/net_benchmark_test.py`, Python's `unittest`, run by ctest as
  `net_benchmark_report` when Python 3 is found): a label compared with the first, the
  page rewritten between its markers only and the same on a second run, the charts of
  both figures, a run replacing only its own lines, the changes, no results.

## Changed
- `run --label <name>` (default `baseline`): a `label` column in the CSV, and a run now
  replaces the lines of its label and profiles instead of the whole file.
- *Getting started*: `--label`, `report` and the link to the page.
- Changelog: #53 merged on 2026-10-29 (`024b3a8`), #54 (`49f9c0e`) and #55 (`8db2fb7`),
  with their last commit.

## Commits
- `2951450` feat(tools): added the report mode of the network benchmark
- `e498317` feat(tests): added tests of the network benchmark report
- feat(docs): documented the network benchmark report and updated changelogs
- feat(changelog): updated changelogs

## Notes
- Only the `lan` profile is measured so far: the degraded ones need root for netem, which
  the machine used did not give. A run with `sudo` (`python3 tools/net_benchmark.py run
  --build <build> --name "NixOS, Ryzen 5 7640U"`, then `report`) completes the table; the
  same for each machine of the team. The `lan` numbers come from the `server`
  configuration (Debug), which changes nothing to the bytes and little to the RTT on the
  loopback.
- Verified: 165 tests (`server` configuration against the engine's `develop`; the new
  Python ones as well with `python3 -m unittest`), clang-format, `mkdocs build --strict`;
  no C++ changed, so no new clang-tidy run. The charts were rendered to PNG and looked at, with
  one run and with three.
- `a flooding address is cut off, and the players play on` fails on the machine used, with
  builds of 2026-10-17 too (environment, see #52); it passes in CI.
