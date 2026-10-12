# Changelog

All notable changes to the R-Type game. Each pull request has its own file in
[`changelogs/`](changelogs/); this index lists them, newest first.

| Pull request | Title | Branch | Status | Date | Details |
|---|---|---|---|---|---|
| — | Follow the engine's develop branch | `build/engine-develop` | Open | 2026-10-12 | [2026-10-12-engine-develop](changelogs/2026-10-12-engine-develop.md) |
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
