# Best scores in the lobby brought to develop

| | |
|---|---|
| Pull request | [#97](https://github.com/Aegis2D-Dev/project-rtype/pull/97) (opened 2026-11-07) |
| Branch | `chore/client-lobby-scoreboard-to-develop` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | [Aegis2D-Dev/aegis2d#313](https://github.com/Aegis2D-Dev/aegis2d/issues/313) feat(client): scoreboard in the lobby |

## Summary
The stacked pull request #96 was merged into its base branch, `feat/server-scoreboard`, a few
seconds after #95 reached `develop` and before #96 was retargeted, so its code never reached
`develop`. This pull request brings it there as reviewed, with nothing new: the best scores of
the server shown on the room list screen (`ListScores` and `ScoreList`, RFC section 3.9).

## Added
- Nothing beyond #96.

## Changed
- Changelog: #95 marked as merged (`6ceb348`), #96 as merged into `feat/server-scoreboard`
  (`c7033b0`) and reaching `develop` through this pull request; the note on the Windows job of
  #96 no longer calls it a draft.

## Commits
- `bafbcc0` Merge remote-tracking branch 'origin/develop' into chore/client-lobby-scoreboard-to-develop
- feat(changelog): updated changelogs

## Notes
- The branch starts at the merge of #96 (`c7033b0`), with `develop` merged in: it differs from
  `develop` by exactly #96.
- Keep `feat/server-scoreboard` until this pull request is merged; then it and
  `feat/client-lobby-scoreboard` can be deleted.
