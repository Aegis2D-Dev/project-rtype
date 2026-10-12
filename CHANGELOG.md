# Changelog

All notable changes to the R-Type game. Each pull request has its own file in
[`changelogs/`](changelogs/); this index lists them, newest first.

| Pull request | Title | Branch | Status | Date | Details |
|---|---|---|---|---|---|
| [#15](https://github.com/Aegis2D-Dev/project-rtype/pull/15) | Linux CI on self-hosted runners | `ci/self-hosted-runners` | Open | 2026-10-12 | [2026-10-12-ci-self-hosted-runners](changelogs/2026-10-12-ci-self-hosted-runners.md) |
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
