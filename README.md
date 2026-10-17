# R-Type

A networked remake of the R-Type horizontal shoot'em up: up to four players, each on their
own client, fight waves of Bydos on a server that has the final say on everything that
happens. The game is built on [Aegis2D](https://github.com/Aegis2D-Dev/aegis2d), our own
C++26 engine, developed alongside it.

> **Status:** playable. Up to four players join a server from their own client and fight
> the waves of the first stage together (see [how to play](docs/play.md) and the
> [changelog](CHANGELOG.md)).

## Binaries

| Binary | Role | Engine modules |
|---|---|---|
| `r-type_server` | Authoritative simulation of the game at 60 ticks per second | core, ecs, net |
| `r-type_client` | Window, input, rendering and sound of one player | every module |

Both accept `--version`, which prints the version and exits. The server listens on TCP port
4242 and UDP port 4243, or on the ports given with `--tcp-port` and `--udp-port` (see
[Getting started](docs/getting-started.md#run)).

Each [release](https://github.com/Aegis2D-Dev/project-rtype/releases) has a package for
Linux (`.tar.gz`) and Windows (`.zip`): unpack it and run the binaries from its `bin`
folder, next to their assets and the engine libraries they need. `cpack` in a build folder
makes the same package in `packages/`.

## Requirements

Linux is the main platform (server and client); Windows is supported, macOS is used for
development.

- CMake 3.30 or newer and Ninja.
- A compiler with C++26 support in CMake: Clang 18+, GCC 14+ or Apple Clang 17+.
  Windows: Visual Studio 2022 or newer with the *C++ Clang tools for Windows* component
  and CMake 4.
- Git access to the engine: [Aegis2D-Dev/aegis2d](https://github.com/Aegis2D-Dev/aegis2d)
  is private and fetched at configure time, so give git your GitHub credentials once
  (e.g. `gh auth setup-git`).
- Client builds: Python 3 with Jinja2 (the engine generates its OpenGL loader), and on
  Linux the X11, OpenGL and audio development packages SDL3 needs:
  ```bash
  python3 -m venv .venv && source .venv/bin/activate
  pip install -r cmake/requirements.txt
  sudo apt-get install libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
      libxfixes-dev libxss-dev libxtst-dev libgl1-mesa-dev libasound2-dev libpulse-dev
  ```

Every library (the engine, SDL3, Asio, Catch2...) is downloaded at configure time with
CMake's FetchContent and [CPM](https://github.com/cpm-cmake/CPM.cmake); nothing is
installed on the system.

## Build, test and run

```bash
cmake --preset linux-debug
cmake --build build/linux-debug
ctest --test-dir build/linux-debug --output-on-failure
./build/linux-debug/r-type_server
./build/linux-debug/r-type_client
```

| Preset | Contents |
|---|---|
| `linux-debug`, `linux-release` | Server, client and tests |
| `server`, `server-release` | Server and tests only, without SDL3 or OpenGL |
| `macos` | Server, client and tests with OpenGL 4.1 (the highest macOS supports) |
| `windows` | Visual Studio and clang-cl; build with the `windows-debug` or `windows-release` build presets |

Builds go to `build/<preset>/`. If CMake does not find Jinja2, add
`-DPython_EXECUTABLE=$PWD/.venv/bin/python` to the first configure.

| Option | Default | Effect |
|---|---|---|
| `RTYPE_BUILD_CLIENT` | `ON` | Build `r-type_client` and the engine's client modules |
| `RTYPE_BUILD_TESTS` | `ON` when built alone | Build the Catch2 tests (`rtype_tests`) |
| `AEGIS2D_WARNINGS_AS_ERRORS` | `OFF` | Turn compiler warnings into errors (CI turns it on) |
| `AEGIS2D_SANITIZE` | `OFF` | ASan and UBSan on the engine and the game |

## Documentation

- [Documentation site](https://rtype-docs.pages.dev/): getting started, architecture,
  gameplay and how-to guides (sources in `docs/`), and the
  [API reference](https://rtype-docs.pages.dev/api/) of the game's headers.
- [Engine documentation](https://aegis2d-docs.pages.dev/): the engine's modules, the
  technical study and the network protocol RFC.
- [CONTRIBUTING.md](CONTRIBUTING.md): tickets, the engine dependency, commits, local
  checks and pull requests.
- [CHANGELOG.md](CHANGELOG.md): one entry per pull request.

## Repository layout

```
game/           rtype_game: entities, collision layers, tuning, the server simulation
protocol/       rtype_protocol: the messages, snapshots and events shared by server and client
server/         r-type_server
client/         r-type_client: window, screens, drawing; client/network: its session
                with the server, its copy of the world, the connect form
assets/         enemy waves (server), sprites and sounds (client)
tests/          Catch2 unit tests of the game
docs/           documentation site and Doxygen configuration
tools/          format and lint scripts, pinned tool versions
changelogs/     one changelog file per pull request
```

## License

R-Type is released under the [MIT License](LICENSE), like the engine. Dependencies keep
their own licenses (see the
[engine README](https://github.com/Aegis2D-Dev/aegis2d#license)).

## Team

[@Neptnium](https://github.com/Neptnium),
[@vmbr068](https://github.com/vmbr068) and
[@Loic-ally](https://github.com/Loic-ally).
