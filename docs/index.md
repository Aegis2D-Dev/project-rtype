# R-Type

A networked remake of the R-Type horizontal shoot'em up: up to four players, each on their
own client, fight waves of Bydos on a server that has the final say on everything that
happens. The game is built on [Aegis2D](https://aegis2d-docs.pages.dev/), our own C++26
engine, developed alongside it.

!!! note "Status"
    Playable: up to four players join a server from their own client and fight the waves
    of the first stage together. See [Play](play.md) and the
    [changelog](https://github.com/Aegis2D-Dev/project-rtype/blob/develop/CHANGELOG.md).

## Binaries

| Binary | Role | Engine modules |
|---|---|---|
| `r-type_server` | Authoritative simulation of the game at 60 ticks per second | core, ecs, shmup, net |
| `r-type_client` | Window, input, rendering and sound of one player | every module |

## Where to go next

- [Getting started](getting-started.md): requirements, access to the engine, build, tests
  and presets.
- [Play](play.md): start a game, the controls, the screens, and the settings.
- [Architecture](architecture.md): how the game sits on the engine, what runs on the
  server and on the client, and how the code is organized.
- [Gameplay](gameplay.md): the simulation tick by tick, who can hit whom, the entities
  and the values they are tuned with.
- [How-to guides](how-to.md): change a gameplay value, add an enemy, add a system, write
  a test.
- [API reference](https://rtype-docs.pages.dev/api/): every header of the game, generated
  by Doxygen.
- [Engine documentation](https://aegis2d-docs.pages.dev/): the engine's modules, the
  technical study and the network protocol RFC.
- [Contributing](https://github.com/Aegis2D-Dev/project-rtype/blob/develop/CONTRIBUTING.md):
  tickets, commits, local checks and pull requests.
