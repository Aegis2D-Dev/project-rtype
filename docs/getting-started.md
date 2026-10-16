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
| `--help` | | Prints the usage and exits |

A port is a number from 0 to 65535; 0 lets the system pick a free one, printed at start. A
bad command line exits with status 2, a bad waves file or a port already in use with
status 1.

For now the server creates a game, loads its waves, lists its systems, then listens on both
ports until stopped with Ctrl+C. Up to 4 players log in over TCP (`LoginRequest`, then
`LoginAccepted` with a session token, or `LoginRejected`), then bind their UDP endpoint
with a `Connect` answered by `Accept`, as section 3 of the protocol RFC specifies. A bound
player joins the game with the ship of its player number, and leaves it when its session
ends; when the last one leaves, a fresh game waits for the next players. The game runs on
its own thread at 60 ticks per second, with or without players, and logs its measured rate
every 10 seconds (`simulation: 600 ticks in 10.0 s (60.0 Hz)`). The most recent `Input` of
each player steers its ship; `Heartbeat` keeps the session acknowledged and `Disconnect`
ends it. A client whose TCP connection closes, even a killed process, leaves the game at
once and its ship disappears for the others. Datagrams from unbound endpoints, or with a
malformed payload, are dropped. Every
bound player receives a snapshot of the world 20 times per second and, resent until they
are acknowledged, the events of the game: each entity that appears (a shot is the `Spawn`
of its missile) or disappears, and each life lost; a player who joins first gets every
entity already there. The client lists the
engine modules it is built with; the window comes with the next tickets.

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
