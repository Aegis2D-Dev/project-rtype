# Changelog

All notable changes to the R-Type game. Each pull request has its own file in
[`changelogs/`](changelogs/); this index lists them, newest first.

| Pull request | Title | Branch | Status | Date | Details |
|---|---|---|---|---|---|
| [#1](https://github.com/Aegis2D-Dev/project-rtype/pull/1) | Bootstrap R-Type on Aegis2D and request the Epitech mirror | `develop` → `master` | Merged | 2026-09-30 | [initial-game-setup](changelogs/initial-game-setup.md) |

## Adding an entry

One pull request = one changelog file.

1. Create `changelogs/<short-slug>.md` in the pull request's branch, from the template below.
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
