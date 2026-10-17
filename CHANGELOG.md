# Changelog

All notable changes to the R-Type game. Each pull request has its own file in
[`changelogs/`](changelogs/); this index lists them, newest first.

| Pull request | Title | Branch | Status | Date | Details |
|---|---|---|---|---|---|
| [#38](https://github.com/Aegis2D-Dev/project-rtype/pull/38) | Client login and UDP binding | `feat/client-session` | Open | 2026-10-17 | [2026-10-17-client-session](changelogs/2026-10-17-client-session.md) |
| [#37](https://github.com/Aegis2D-Dev/project-rtype/pull/37) | Per-address packet and connection limits | `feat/server-rate-limit` | Merged | 2026-10-17 | [2026-10-17-server-rate-limit](changelogs/2026-10-17-server-rate-limit.md) |
| [#36](https://github.com/Aegis2D-Dev/project-rtype/pull/36) | Clean stop on SIGINT and SIGTERM | `feat/server-graceful-shutdown` | Merged | 2026-10-17 | [2026-10-17-server-graceful-shutdown](changelogs/2026-10-17-server-graceful-shutdown.md) |
| [#35](https://github.com/Aegis2D-Dev/project-rtype/pull/35) | Errors of one client contained | `fix/server-client-errors` | Merged | 2026-10-16 | [2026-10-16-server-client-errors](changelogs/2026-10-16-server-client-errors.md) |
| [#34](https://github.com/Aegis2D-Dev/project-rtype/pull/34) | Players told who joins and who leaves | `feat/server-player-left` | Merged | 2026-10-16 | [2026-10-16-server-player-left](changelogs/2026-10-16-server-player-left.md) |
| [#33](https://github.com/Aegis2D-Dev/project-rtype/pull/33) | Disconnect detected when UDP goes silent | `feat/server-udp-timeout` | Merged | 2026-10-16 | [2026-10-16-server-udp-timeout](changelogs/2026-10-16-server-udp-timeout.md) |
| [#32](https://github.com/Aegis2D-Dev/project-rtype/pull/32) | Disconnect detected when the TCP connection closes | `feat/server-tcp-disconnect` | Merged | 2026-10-16 | [2026-10-16-server-tcp-disconnect](changelogs/2026-10-16-server-tcp-disconnect.md) |
| [#31](https://github.com/Aegis2D-Dev/project-rtype/pull/31) | Gameplay events sent to the players | `feat/server-events` | Merged | 2026-10-16 | [2026-10-16-server-events](changelogs/2026-10-16-server-events.md) |
| [#30](https://github.com/Aegis2D-Dev/project-rtype/pull/30) | Snapshots broadcast at 20 Hz | `feat/server-snapshots` | Merged | 2026-10-15 | [2026-10-15-server-snapshots](changelogs/2026-10-15-server-snapshots.md) |
| [#29](https://github.com/Aegis2D-Dev/project-rtype/pull/29) | Client inputs applied to player ships | `feat/server-inputs` | Merged | 2026-10-15 | [2026-10-15-server-inputs](changelogs/2026-10-15-server-inputs.md) |
| [#28](https://github.com/Aegis2D-Dev/project-rtype/pull/28) | 60 Hz simulation thread | `feat/server-simulation-thread` | Merged | 2026-10-15 | [2026-10-15-server-simulation-thread](changelogs/2026-10-15-server-simulation-thread.md) |
| [#27](https://github.com/Aegis2D-Dev/project-rtype/pull/27) | Game instance with up to 4 players | `feat/server-game-instance` | Merged | 2026-10-15 | [2026-10-15-server-game-instance](changelogs/2026-10-15-server-game-instance.md) |
| [#26](https://github.com/Aegis2D-Dev/project-rtype/pull/26) | UDP endpoints bound to sessions | `feat/server-udp-binding` | Merged | 2026-10-15 | [2026-10-15-server-udp-binding](changelogs/2026-10-15-server-udp-binding.md) |
| [#25](https://github.com/Aegis2D-Dev/project-rtype/pull/25) | TCP login issuing a session token | `feat/server-login` | Merged | 2026-10-15 | [2026-10-15-server-login](changelogs/2026-10-15-server-login.md) |
| [#24](https://github.com/Aegis2D-Dev/project-rtype/pull/24) | Server entry point with port arguments | `feat/server-entry-point` | Merged | 2026-10-15 | [2026-10-15-server-entry-point](changelogs/2026-10-15-server-entry-point.md) |
| [#23](https://github.com/Aegis2D-Dev/project-rtype/pull/23) | Guide: add an enemy type | `docs/add-enemy-type` | Merged | 2026-10-14 | [2026-10-14-add-enemy-type](changelogs/2026-10-14-add-enemy-type.md) |
| [#22](https://github.com/Aegis2D-Dev/project-rtype/pull/22) | Enemy waves loaded from a JSON file | `feat/enemy-waves` | Merged | 2026-10-14 | [2026-10-14-enemy-waves](changelogs/2026-10-14-enemy-waves.md) |
| [#21](https://github.com/Aegis2D-Dev/project-rtype/pull/21) | Bydos slaves fire enemy missiles | `feat/enemy-missiles` | Merged | 2026-10-14 | [2026-10-14-enemy-missiles](changelogs/2026-10-14-enemy-missiles.md) |
| [#20](https://github.com/Aegis2D-Dev/project-rtype/pull/20) | Incremental Linux builds on pull requests | `ci/linux-build-cache` | Merged | 2026-10-13 | [2026-10-13-ci-linux-build-cache](changelogs/2026-10-13-ci-linux-build-cache.md) |
| [#19](https://github.com/Aegis2D-Dev/project-rtype/pull/19) | Random Bydos spawns on the right edge | `feat/random-enemy-spawns` | Merged | 2026-10-13 | [2026-10-13-random-enemy-spawns](changelogs/2026-10-13-random-enemy-spawns.md) |
| [#18](https://github.com/Aegis2D-Dev/project-rtype/pull/18) | CI build caches on self-hosted runners | `ci/windows-build-cache` | Merged | 2026-10-13 | [2026-10-13-ci-windows-build-cache](changelogs/2026-10-13-ci-windows-build-cache.md) |
| [#17](https://github.com/Aegis2D-Dev/project-rtype/pull/17) | Bydos slaves weave in a sine wave | `feat/bydos-sine-wave` | Merged | 2026-10-13 | [2026-10-13-bydos-sine-wave](changelogs/2026-10-13-bydos-sine-wave.md) |
| [#16](https://github.com/Aegis2D-Dev/project-rtype/pull/16) | Missiles and enemies despawn off screen | `feat/offscreen-despawn` | Merged | 2026-10-13 | [2026-10-13-offscreen-despawn](changelogs/2026-10-13-offscreen-despawn.md) |
| [#15](https://github.com/Aegis2D-Dev/project-rtype/pull/15) | CI on self-hosted runners | `ci/self-hosted-runners` | Merged | 2026-10-12 | [2026-10-12-ci-self-hosted-runners](changelogs/2026-10-12-ci-self-hosted-runners.md) |
| [#12](https://github.com/Aegis2D-Dev/project-rtype/pull/12) | README, documentation site and API reference | `docs/documentation` | Merged | 2026-10-12 | [2026-10-12-documentation](changelogs/2026-10-12-documentation.md) |
| [#11](https://github.com/Aegis2D-Dev/project-rtype/pull/11) | Contributing guide and GitHub templates | `docs/contributing` | Merged | 2026-10-12 | [2026-10-12-contributing](changelogs/2026-10-12-contributing.md) |
| [#10](https://github.com/Aegis2D-Dev/project-rtype/pull/10) | Code quality CI | `ci/code-quality` | Merged | 2026-10-12 | [2026-10-12-ci-code-quality](changelogs/2026-10-12-ci-code-quality.md) |
| [#9](https://github.com/Aegis2D-Dev/project-rtype/pull/9) | Format and lint tooling | `chore/tooling` | Merged | 2026-10-12 | [2026-10-12-tooling](changelogs/2026-10-12-tooling.md) |
| [#8](https://github.com/Aegis2D-Dev/project-rtype/pull/8) | Player missiles with a fire cooldown | `feat/player-weapons` | Merged | 2026-10-12 | [2026-10-12-player-weapons](changelogs/2026-10-12-player-weapons.md) |
| [#7](https://github.com/Aegis2D-Dev/project-rtype/pull/7) | Server gameplay on the engine systems | `feat/server-gameplay` | Merged | 2026-10-12 | [2026-10-12-server-gameplay](changelogs/2026-10-12-server-gameplay.md) |
| [#6](https://github.com/Aegis2D-Dev/project-rtype/pull/6) | Follow the engine's develop branch | `build/engine-develop` | Merged | 2026-10-12 | [2026-10-12-engine-develop](changelogs/2026-10-12-engine-develop.md) |
| [#4](https://github.com/Aegis2D-Dev/project-rtype/pull/4) | Replace rtype with r-type_server and r-type_client, add presets | `build/executables` | Merged | 2026-10-02 | [2026-10-02-executables](changelogs/2026-10-02-executables.md) |
| [#2](https://github.com/Aegis2D-Dev/project-rtype/pull/2) | Name changelog files by date | `docs/changelog-dated-names` | Merged | 2026-10-01 | [2026-10-01-changelog-dated-names](changelogs/2026-10-01-changelog-dated-names.md) |
| [#1](https://github.com/Aegis2D-Dev/project-rtype/pull/1) | Bootstrap R-Type on Aegis2D and request the Epitech mirror | `develop` → `master` | Merged | 2026-09-30 | [2026-09-30-initial-game-setup](changelogs/2026-09-30-initial-game-setup.md) |

## Adding an entry

One pull request = one changelog file.

1. Create `changelogs/YYYY-MM-DD-<short-slug>.md` in the pull request's branch, from the
   template below, dated the day the pull request is opened (e.g.
   `2026-10-01-server-entry-point.md`), so the folder lists them in date order.
2. Add a row at the top of the table above, linking to the file.
3. When the pull request is opened, fill in its number; when it is merged, update the status and date.

```markdown
# <Pull request title>

| | |
|---|---|
| Pull request | [#N](https://github.com/Aegis2D-Dev/project-rtype/pull/N) |
| Branch | `<branch>` → `develop` |
| Status | Open / Merged on YYYY-MM-DD |
| Author | @handle |
| Ticket | #N |

## Summary
One or two sentences: what changes and why.

## Added
## Changed
## Fixed
## Removed
(keep only the sections that apply)

## Commits
- `abc1234` type(scope): message

## Notes
Migration steps, follow-ups, known limitations.
```
