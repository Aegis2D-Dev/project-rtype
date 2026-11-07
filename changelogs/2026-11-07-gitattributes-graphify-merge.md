# Graphify merge driver for the graph output

| | |
|---|---|
| Pull request | [#91](https://github.com/Aegis2D-Dev/project-rtype/pull/91) (opened 2026-11-07) |
| Branch | `feat/gitattributes-graphify-merge` → `develop` |
| Status | Open |
| Author | @vmbr068 |
| Ticket | None: graphify tooling |

## Summary
A new `.gitattributes` merges `graphify-out/graph.json` with the `graphify` merge driver
instead of the text merge. Same tooling in the engine: Aegis2D-Dev/aegis2d#475.

## Added
- `.gitattributes`: `graphify-out/graph.json merge=graphify`.

## Changed
- Nothing.

## Commits
- `8e82049` feat(gitattributes): added graphify merge driver for graph output
- feat(changelog): updated changelogs

## Notes
- `git check-attr merge graphify-out/graph.json` returns `graphify`. No code changed:
  nothing to build or test.
- `graphify-out/` is in `.gitignore`, so the rule only applies on a machine that tracks it.
  Without the `graphify` driver configured, git falls back to its text merge.
