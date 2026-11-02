# Compact and delta snapshots brought to develop

| | |
|---|---|
| Pull request | [#61](https://github.com/Aegis2D-Dev/project-rtype/pull/61) (opened 2026-11-02) |
| Branch | `feat/protocol-snapshot-baselines` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#281](https://github.com/Aegis2D-Dev/aegis2d/issues/281), [#282](https://github.com/Aegis2D-Dev/aegis2d/issues/282), [#283](https://github.com/Aegis2D-Dev/aegis2d/issues/283) (merged already, through #58, #59 and #60) |

## Summary
The stacked pull requests #58, #59 and #60 were merged into their base branches before
GitHub retargeted them to `develop`, so only #57 reached it. This pull request brings their
code to `develop` as reviewed, with nothing new: quantized snapshot records (#58), snapshot
acknowledgement and baselines (#59) and the delta snapshot encoder (#60).

## Added
- Nothing beyond #58, #59 and #60.

## Changed
- Changelog: #57 marked as merged into `develop` (`1bd6796`); #58 (`368d17d`), #59
  (`df300c0`) and #60 (`ee393d6`) marked as merged into their base branches, reaching
  `develop` through this pull request.

## Commits
- `ccb2cb8` Merge remote-tracking branch 'origin/develop' into feat/protocol-snapshot-baselines
- feat(changelog): fixed pr numbers and merge status

## Notes
- `feat/protocol-snapshot-baselines` holds the commits of #58, #59 and #60 (each stacked
  on the previous one) and the merge of #60; `develop` merged in, it differs from `develop`
  by exactly those three pull requests.
- Keep the branch until this pull request is merged; then `feat/protocol-compact-encoders`,
  `feat/protocol-quantized-snapshots`, `feat/protocol-snapshot-baselines` and
  `feat/server-delta-snapshots` can be deleted.
