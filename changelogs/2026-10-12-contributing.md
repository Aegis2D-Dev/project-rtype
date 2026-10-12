# Contributing guide and GitHub templates

| | |
|---|---|
| Pull request | [#11](https://github.com/Aegis2D-Dev/project-rtype/pull/11) (opened 2026-10-12) |
| Branch | `docs/contributing` → `develop` |
| Status | Merged on 2026-10-12 with #12 (`83d0bed`) |
| Author | @Loic-ally |

## Summary
The game gets its contributing guide and GitHub forms. The workflow is the engine's
(branches, commits, reviews, merge commits), so the guide links to it and only covers
what differs here: tickets kept in the engine repository, the dependency on the engine's
`develop`, the static game library, the scopes, the local checks and the presets.

## Added
- `CONTRIBUTING.md`: tickets (in `Aegis2D-Dev/aegis2d`, named `Closes
  Aegis2D-Dev/aegis2d#N` or `Refs ...`), the engine (`develop`, engine pull request first,
  private repository, game code reading components kept out of shared libraries),
  commit scopes, local checks (presets, tests, `tools/format.sh`, `tools/lint.sh`,
  sanitizers), pull requests and code style.
- `.github/PULL_REQUEST_TEMPLATE.md`: the engine's template, with tickets in the engine
  repository, this repository's guide, and a box for the engine pull request.
- `.github/ISSUE_TEMPLATE/bug.yml`: the engine's bug form, with game steps and the engine
  commit the game was built with.
- `.github/ISSUE_TEMPLATE/config.yml`: no blank issues; new tasks go to the engine
  repository, plus a link to the guide.

## Commits
- feat(docs): added contributing guide and github templates
- feat(changelog): updated changelogs

## Notes
- No *Task* form here on purpose: tasks stay on one board, in the engine repository.
- Made from `ci/code-quality` (#10, not merged yet).
