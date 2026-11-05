# No build for documentation-only pull requests

| | |
|---|---|
| Pull request | [#81](https://github.com/Aegis2D-Dev/project-rtype/pull/81) (opened 2026-11-05) |
| Branch | `ci/windows-runner-load` → `develop` |
| Status | Merged on 2026-11-05 (`d348f01`) |
| Author | @vmbr068 |
| Ticket | None: *Code quality* waiting up to 2 hours for a Windows runner on 2026-11-05 |

## Summary
The Windows job of *Code quality* waits for the 2 Windows runners, shared with the engine:
on 2026-11-05 it waited 27 minutes on average and up to 112 minutes, for 3 to 7 minutes of
work. A pull request that only changes the documentation no longer runs *Code quality*, so it
takes no runner. The engine also runs one Windows job instead of two on its pull requests:
Aegis2D-Dev/aegis2d#457.

## Added
- Nothing.

## Changed
- *Code quality*: a pull request that only changes `docs/`, `changelogs/`, Markdown files or
  `mkdocs.yml` does not run it; the *Documentation* workflow checks it. Pushes to `develop`
  and `master` still run everything.
- *Contributing*: the checks of a pull request.
- Changelog: #74 (`693e0f7`) marked as merged.

## Commits
- `1fea01a` feat(ci): skipped the build of pull requests that only change docs
- feat(docs): documented the lighter checks and updated changelogs

## Notes
- No test of the game reads `docs/`, so skipping the build loses no check.
- Not run before the pull request: GitHub Actions runs on GitHub only. The workflow file was
  parsed as YAML.
- With no branch protection on the repository, a workflow not run blocks no merge.
