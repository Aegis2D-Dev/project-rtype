# LZ4 compression of snapshots and events, measured

| | |
|---|---|
| Pull request | [#63](https://github.com/Aegis2D-Dev/project-rtype/pull/63) (opened 2026-11-02) |
| Branch | `feat/server-payload-compression` → `develop` |
| Status | Merged on 2026-11-02 (`f4ade28`) |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#287](https://github.com/Aegis2D-Dev/aegis2d/issues/287) feat(net): payload compression |

## Summary
With `--compress <bytes>`, the server sends its snapshots and events of at least that many
bytes compressed with the engine's LZ4 (Aegis2D-Dev/aegis2d#432) when that makes them
smaller; the client and the server decompress whatever arrives flagged. The benchmark
measures the game with and without it: on the `lan` profile, with the delta snapshots of
#62, 4544.42 bytes per second per bot without compression and 4725.97 with it, a difference
within the spread between two runs; before the deltas, 7067.90 against 6859.77 (-2.9 %).
Compression stays off by default. Tenth ticket of the *Network: data transmission efficiency* epic
(Aegis2D-Dev/aegis2d#277).

## Added
- `r-type_server --compress <bytes>` (1 to 1182, off by default): `ServerOptions::compress_from`,
  `ServerNetwork::set_compression()` and `compression()`. Snapshots go through
  `net::write_datagram()`; the `Event` datagrams of `ReliableChannel` are written again with
  their payload compressed.
- On both sides, a flagged payload is decompressed with `net::read_payload()` before anything
  reads it; one that does not decompress is dropped (`malformed compressed payload` in the
  server's drop report), and the client does not acknowledge it.
- `tools/net_benchmark.py run --compress <bytes>`: starts the server with that option.
- Benchmark results (`lan`, NixOS, Ryzen 5 7640U): `quantized-snapshots`, the game before
  #62, `delta-snapshots`, the game with #62 and without compression, and `lz4`, the same with
  `--compress 128`.
- Tests: the option and its bounds; through sockets, two snapshot payloads sent plain then
  compressed from 128 bytes (the one of 122 records compressed and smaller, the one of 8 below
  the threshold), each read back whole, and a broken compressed datagram dropped; a client
  acknowledging a compressed snapshot of a real server, which it could only do after reading
  it. A benchmark run that keeps the other labels of its CSV.

## Changed
- `tools/net_benchmark.py run` emptied its CSV before reading it, so each run erased the
  lines of the other labels: it now reads them first.
- *Getting started*: the server option and the benchmark option. *Network benchmark*: the
  new runs and charts.
- Snapshots, encoded for each player against its baseline since #62, go through
  `net::write_datagram()` too; the compressed snapshot test of the client goes through that
  path (`send_snapshot(snapshot)`), as the client no longer reads the full layout.
- Changelog: #61 (`2bd6f69`) and #62 (`bbc3aa3`) marked as merged.

## Commits
- `4bb7fcd` feat(server): compressed snapshots and events with lz4 from a threshold
- `46a6c7b` feat(tests): added compressed snapshot and option tests
- `04c76ee` feat(tools): kept the other labels of a benchmark csv when writing a run
- `4435f24` feat(tests): added a benchmark run test keeping the other labels
- `6c00e9d` feat(docs): documented the compression option and updated changelogs
- `d590e6d` Merge remote-tracking branch 'origin/develop' into feat/server-payload-compression
- `a42be75` feat(tests): sent the compressed snapshot test through delta encoding
- feat(docs): measured compression with delta snapshots and updated changelogs

## Notes
- Needed Aegis2D-Dev/aegis2d#432, merged since (`018e5f1`): the CI of the first commits
  failed on the missing `aegis2d/net/compression.hpp`.
- Before #62, two runs without compression gave 7436.65 and 7067.90 bytes per second (the
  game is not exactly the same from one run to the other), two with it 6877.97 and 6859.77.
  With #62, LZ4 has nothing left to find in the deltas: the run with it received more, which
  only shows the spread between runs (a payload is sent compressed only when it shrinks).
  The `lz4` line of the CSV is the one measured with #62. The engine's study measures each
  technique on the same snapshots (Aegis2D-Dev/aegis2d#433).
- Only the `lan` profile was measured: the others need netem, so root.
- The runs used a Debug build (`server` preset); byte counts do not depend on the build type.
- Verified with the engine of #432 (`server` preset, Clang 20, warnings as errors): build,
  `tools/format.sh`, `tools/lint.sh` (clang-tidy 22), the benchmark's Python tests,
  `mkdocs build --strict`.
- `ctest`: 204 of 205 tests pass. *a flooding address is cut off, and the players play on*
  fails on the machine used, as on `develop`, and passes in CI.
