# Per-address packet and connection limits

| | |
|---|---|
| Pull request | [#37](https://github.com/Aegis2D-Dev/project-rtype/pull/37) (opened 2026-10-17) |
| Branch | `feat/server-rate-limit` → `develop` |
| Status | Merged on 2026-10-17 (`d599aff`) |
| Author | @vmbr068 |
| Ticket | Aegis2D-Dev/aegis2d#120 per-IP packet rate limit |

## Summary
One IP address can no longer flood the server: its datagrams past a rate are dropped before
being read, its connections past a count are closed at once, and what is dropped is summed
up in the log every 10 seconds at most, never one line per datagram (RFC sections 6.6 and
6.7). A flood of 180,000 datagrams per second leaves the simulation at 60 ticks per second.
Last ticket of the game server epic (Aegis2D-Dev/aegis2d#107).

## Added
- `rtype/rate_limit.hpp`: `RateLimiter` (datagrams per address and second, idle addresses
  forgotten), `DropReport` (dropped datagrams by address and reason, summed up every 10
  seconds), `RateLimits` (300 datagrams per second and 8 connections per address by
  default), `address_of()`.
- `ServerNetwork::set_limits()`, `rate_limited()`, `refused_connections()`.
- `--max-packet-rate <n>`: datagrams accepted per second from one address, 1 to 100000.
- Tests: the limiter and the report alone, the option, a flooding address cut off while
  the players play on, the connection limit, and the tick rate under a flood.

## Changed
- `ServerNetwork`: the rate limit is checked before any other check of a datagram; a
  connection past its address's share is closed at once; an address going over the limit
  is logged once, and the dropped datagrams are summed up in `update()`.
- *Getting started*: the option and the limits.
- Changelog: #36 (`94150b0`) marked as merged, with its commits and the fix after its
  first CI run.

## Commits
- `59266fc` feat(server): limited the packets and connections of each address
- `78e9ca8` feat(changelog): updated changelogs

## Notes
- The flood tests send from 127.0.0.2, which Linux and Windows route as loopback.
- Verified: 114 tests, the limit ones 10 times in a row, the suite under ASan and UBSan,
  the server tests under ThreadSanitizer, clang-format and clang-tidy 22 on every new and
  changed file, Doxygen, `mkdocs build --strict`, and a flood against `r-type_server`.
