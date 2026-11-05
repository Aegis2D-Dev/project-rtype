# Getting started

## Requirements

Linux is the main platform (server and client); Windows is supported, macOS is used for
development.

- CMake 3.30 or newer and Ninja.
- A compiler with C++26 support in CMake: Clang 18+, GCC 14+ or Apple Clang 17+.
- Windows: Visual Studio 2022 or newer with the *C++ Clang tools for Windows* component
  (clang-cl 17+) and CMake 4. CMake has no C++26 mode for the plain MSVC compiler.
- Client builds only: Python 3 with Jinja2, because the engine generates its OpenGL
  loader with glad:
  ```bash
  python3 -m venv .venv && source .venv/bin/activate
  pip install -r cmake/requirements.txt
  ```
- Linux client builds: the X11, OpenGL and audio development packages SDL3 needs, e.g. on
  Ubuntu:
  ```bash
  sudo apt-get install libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
      libxfixes-dev libxss-dev libxtst-dev libgl1-mesa-dev libasound2-dev libpulse-dev
  ```

Every library is downloaded at configure time: the engine with FetchContent, its
dependencies (SDL3, Asio, GLM...) and Catch2 with
[CPM](https://github.com/cpm-cmake/CPM.cmake). Nothing is installed on the system.

## Access to the engine

The engine repository, [Aegis2D-Dev/aegis2d](https://github.com/Aegis2D-Dev/aegis2d), is
private. Give git your GitHub credentials once, for example with the GitHub CLI:

```bash
gh auth login
gh auth setup-git
```

Development builds follow the engine's `develop` branch (`RTYPE_ENGINE_BRANCH` in
`CMakeLists.txt`), so an engine feature reaches the game as soon as it is merged there:
each configure asks git for the latest commit of the branch (`Engine: develop is at ...`)
and fetches it if the build directory has an older one. If git cannot reach the engine, a
warning says so and the build directory keeps the engine it has.

## Build and test

```bash
cmake --preset linux-debug
cmake --build build/linux-debug
ctest --test-dir build/linux-debug --output-on-failure
```

| Configure preset | Contents |
|---|---|
| `linux-debug`, `linux-release` | Server, client and tests |
| `server`, `server-release` | Server and tests only, without SDL3 or OpenGL |
| `macos` | Server, client and tests with OpenGL 4.1 (the highest macOS supports) |
| `windows` | Visual Studio and clang-cl, multi-configuration |

On Windows, build with `cmake --build --preset windows-debug` (or `windows-release`) and
test with `ctest --test-dir build/windows -C Debug`.

If CMake does not find Jinja2, add `-DPython_EXECUTABLE=$PWD/.venv/bin/python` to the
first configure.

### Options

| Option | Default | Effect |
|---|---|---|
| `RTYPE_BUILD_CLIENT` | `ON` | Build `r-type_client` and the engine's client modules |
| `RTYPE_BUILD_TESTS` | `ON` when built alone | Build the Catch2 tests (`rtype_tests`) |
| `AEGIS2D_WARNINGS_AS_ERRORS` | `OFF` | Turn compiler warnings into errors (CI turns it on) |
| `AEGIS2D_SANITIZE` | `OFF` | ASan and UBSan on the engine and the game |

## Run

```bash
./build/linux-debug/r-type_server
./build/linux-debug/r-type_client
```

Both binaries accept `--version`, which prints the version and exits. The server takes:

| Option | Default | Meaning |
|---|---|---|
| `--tcp-port <port>` | `4242` | TCP port of the logins |
| `--udp-port <port>` | `4243` | UDP port of the game |
| `--waves <file>` | `assets/waves/stage1.json` | Enemy waves to play |
| `--max-packet-rate <n>` | `300` | UDP datagrams accepted per second from one IP address (1 to 100000) |
| `--compress <bytes>` | off | Sends the snapshots and events of at least that many bytes (1 to 1182) compressed with LZ4 when that makes them smaller |
| `--max-rooms <count>` | `4` | Games played at once, 4 players in each (1 to 64): a room opens with its first player and closes with its last |
| `--help` | | Prints the usage and exits |

A port is a number from 0 to 65535; 0 lets the system pick a free one, printed at start. A
bad command line exits with status 2, a bad waves file or a port already in use with
status 1.

Each IP address may send `--max-packet-rate` datagrams per second and hold 8 TCP connections
at once; past that, its datagrams are dropped before being read and its new connections
closed, so a flood does not slow the game. Dropped datagrams are summed up in the log at
most every 10 seconds per address (`UDP: 203.0.113.7: 1150 dropped in 10 s (1150 rate
limit)`), never one line per datagram.

Every 10 seconds the server also logs the traffic of each player: datagrams and bytes per
second over the last whole second, by packet type, the bytes since the player joined, and
the round trip time with its jitter (`UDP: player 0 sends 21 datagrams/s, 3420 B/s
(snapshots 20, events 1), receives 60 datagrams/s, 1140 B/s (inputs 60, events 0,
heartbeats 0); 41200 B sent and 13600 B received in all; round trip 203 ms (jitter 1 ms)`).
Bytes count the datagrams with their 18-byte header, without the IP and UDP headers. The
round trip is measured from the acknowledgements the datagrams already carry, smoothed like
TCP's. Each sample also holds the time the client waits before its next `Input`, up to
17 ms; the client's own estimate, the time the server waits before its next snapshot.

Ctrl+C (SIGINT) or SIGTERM stops the server cleanly: every player gets `Logout` with the
reason `Shutdown`, each connection closes once it has left (1 second at most), then the
server exits with status 0. A second Ctrl+C quits at once, with status 130.

For now the server loads its waves, lists the systems of a game, then listens on both ports
until stopped with Ctrl+C. Players log in over TCP (`LoginRequest`, then `LoginAccepted` with
a session token, or `LoginRejected`), up to 4 per room, then bind their UDP endpoint with a
`Connect` answered by `Accept`, as section 3 of the protocol RFC specifies; a logged-in
client may also list the rooms and create one (section 3.6). A bound player joins the game of
its room with the ship of its player number, and leaves it when its session ends; when the
last one leaves, the room closes, and its next players get a fresh game. Each game runs on
its own thread at 60 ticks per second while it has players, and logs its measured rate
every 10 seconds (`simulation: 600 ticks in 10.0 s (60.0 Hz)`). The most recent `Input` of
each player steers its ship; `Heartbeat` keeps the session acknowledged and `Disconnect`
ends it. A client whose TCP connection closes, even a killed process, leaves the game at
once and its ship disappears for the others; so does a player that sends no valid datagram
for 5 seconds (its UDP blocked, its machine frozen), after a `Logout` with the reason
`TimedOut`. The other players are told who joins (`PlayerJoined`, with the name) and who
leaves and why (`PlayerLeft`), before its ship disappears. A packet the server cannot
handle only gets its sender kicked: the others play on. Datagrams from unbound
endpoints, or with a
malformed payload, are dropped. Every
bound player receives a snapshot of the world 20 times per second and, resent until they
are acknowledged, the events of the game: each entity that appears (a shot is the `Spawn`
of its missile) or disappears, and each life lost; a player who joins first gets every
entity already there.

The client opens a window on its connect screen, filled with `--host <address>` (a
numeric IPv4 or IPv6 address, or `localhost`; `127.0.0.1` by default), `--port <tcp port>`
(`4242`) and `--name <player name>` (`player`); Enter joins, and `--join` joins at once.
[Play](play.md) has its options, the controls and what the screen shows.

```bash
./build/linux-debug/r-type_client --host 127.0.0.1 --port 4242 --name R9
```

### Bots

`r-type_bot` is a player without a window: it joins a server as the client does, then
holds random inputs (a direction or none on each axis, Fire most of the time, each for 150
to 900 ms), or replays a script of inputs, one `Input` per tick. One process can run several
bots. It needs no SDL nor OpenGL, so every preset builds it,
the `server` one included; it serves load and network tests.

```bash
./build/linux-debug/r-type_bot --port 4242 --name B1 --seconds 60
./build/linux-debug/r-type_bot --count 4 --seconds 30   # fills a game for a stage
./build/linux-debug/r-type_bot --count 2 --inputs tools/bot/dodge.txt
```

| Option | Default | Meaning |
|---|---|---|
| `--host <address>` | `127.0.0.1` | Server, as for the client |
| `--port <tcp port>` | `4242` | TCP port of the server |
| `--name <player name>` | `bot` | Player name; two processes on one server need two names |
| `--count <n>` | `1` | Bots run by the process, 1 to 64, named `<name>1`, `<name>2`...; a server takes 4 |
| `--inputs <file>` | random inputs | Script replayed by every bot, from its start in game, in a loop |
| `--seconds <n>` | until Ctrl+C | Play time once in game, then leave |
| `--seed <n>` | drawn at start | Seed of the random inputs: the same seed holds the same inputs; with `--count`, bot `i` takes seed + i - 1 |
| `--stats <file>` | | CSV written at the end, one line per bot that played (below) |
| `--help`, `--version` | | Prints the usage or the version and exits |

An input script holds one step per line, a duration in milliseconds then the actions held:
`U`, `D`, `L`, `R`, `F` (up, down, left, right, fire) or `-` for none; blank lines and lines
starting with `#` are skipped. [`tools/bot/dodge.txt`](https://github.com/Aegis2D-Dev/project-rtype/blob/develop/tools/bot/dodge.txt)
moves forward, dodges while firing, then falls back:

```text
600 RF
400 UF
400 DF
300 -
```

It prints its seed (or script) at start, then the player each bot plays (`bot1 joined
127.0.0.1:4242 as player 1`), and when they leave, what each exchanged (`bot1 left after
30.0 s: 1801 inputs sent (57 holds), 600 snapshots received`). Each bot leaves with
`Logout` after `--seconds` in game, or all of them at Ctrl+C (SIGINT) or SIGTERM; the
process exits with status 0, 1 when a bot cannot join or loses the server (the others play
on), when the script cannot be read, and 2 on a bad command line.

With `--stats`, each bot that played measures its link when it leaves: `bot`, `seconds` in
game, `inputs_sent`, `snapshots` received, `bytes_in_per_s` and `bytes_out_per_s` (the
datagrams with their 18-byte header, averaged over the game), `rtt_ms` and
`rtt_jitter_ms` (the smoothed round trip time at the end), and over the last 2 seconds
`loss_percent` (the server's datagrams lost) and `snapshot_jitter_ms`.

### Network benchmark

`tools/net_benchmark.py` measures the game's network under the conditions players meet:
it starts `r-type_server`, then for each profile degrades the server's two ports on `lo`
with the engine's `tools/netem.sh`, plays `r-type_bot --count 4 --seconds 30 --stats`, and
writes one line per profile, the mean of the four bots, to
`docs/network-benchmark/<machine>.csv`, under a `--label` (`baseline` by default, then the
name of each optimization): a run replaces the lines of its label and keeps the others.
One command measures every profile, another publishes the results on the
[Network benchmark](network-benchmark.md) page:

```bash
python3 tools/net_benchmark.py run --build build/linux-release --name "Arch, Ryzen 7"
python3 tools/net_benchmark.py report
```

| Profile | netem on each packet | Loss |
|---|---|---|
| `lan` | nothing | 0 % |
| `50ms` | 50 ms | 0 % |
| `150ms-2%` | 150 ms | 2 % |
| `300ms-5%` | 300 ms | 5 % |

netem delays each packet, so on `lo` each direction: the round trip grows by twice the
delay, and a datagram is lost with the given chance each way. Changing the queues of `lo`
needs root, so the script runs `netem.sh` with `sudo` (it asks for the password once) and
removes the rules after each profile, even after an error; the `lan` profile needs no
root. `--profiles lan,50ms` picks some, `--bots`, `--seconds` change the game, `--netem`
gives `netem.sh` when the build tree was not configured from the engine's sources, and
`--dry-run` prints the `tc` commands without degrading anything (nothing is written then,
unless `--output` names a file). `--compress <bytes>` starts the server with that option, to
measure the game with and without compression under two labels.

The CSV has, per profile: `machine`, `os`, `label`, `profile`, `delay_ms`, `loss_set_percent`,
`bots`, `seconds`, then the means of the bots' `--stats` columns (`bytes_in_per_s`,
`bytes_out_per_s`, `rtt_ms`, `rtt_jitter_ms`, `loss_percent`, `snapshot_jitter_ms`), and
`corrections`, empty until the client predicts its own ship and counts the corrections of
the server. Only the standard library of Python 3.10 or newer is needed.

## Documentation

The site is built with [MkDocs Material](https://squidfunk.github.io/mkdocs-material/) and
the API reference with Doxygen:

```bash
pip install -r docs/requirements.txt
mkdocs serve                    # http://127.0.0.1:8000, reloads on save
doxygen docs/Doxyfile           # build/doxygen/html/index.html
```

Every push to `develop` that changes the docs or the game's headers deploys the site to
[rtype-docs.pages.dev](https://rtype-docs.pages.dev/), with the API reference under
`/api/`.
