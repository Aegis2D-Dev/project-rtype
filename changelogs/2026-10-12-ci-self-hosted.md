# Linux CI jobs on the self-hosted runner

| | |
|---|---|
| Pull request | [#14](https://github.com/Aegis2D-Dev/project-rtype/pull/14) (opened 2026-10-12) |
| Branch | `ci/self-hosted` → `develop` |
| Status | Open |
| Author | @Loic-ally |

## Summary
Every Linux job now runs on the team's self-hosted runner (`[self-hosted, linux, x64]`),
shared with the engine, instead of GitHub's `ubuntu-24.04`: *Code quality* (format,
build + tests + clang-tidy, sanitizers), *Documentation* (build and deploy) and *Request
Epitech mirror*. The Windows job stays on GitHub's `windows-latest`. The machine, its
registration and its security rules are described on the engine's
[CI runner](https://aegis2d-docs.pages.dev/ci-runner/) page (aegis2d#210).

## Changed
- `.github/workflows/code-quality.yml`: `format`, `build` and `sanitize` on the
  self-hosted runner; the token that fetches the private engine goes to a job-local git
  config in `$RUNNER_TEMP` (`GIT_CONFIG_GLOBAL`) instead of `git config --global`, so it
  never stays in the runner's `~/.gitconfig` (the Windows job, on a fresh GitHub machine,
  keeps `--global`).
- `.github/workflows/docs.yml`: `build` and `deploy` on the self-hosted runner; the deploy
  job cleans its workspace before downloading the site, and installs Node.js 22 with
  `actions/setup-node` (pinned by commit) for wrangler.
- `.github/workflows/request-epitech-mirror.yml`: on the self-hosted runner, and the
  dispatch is sent with `curl` instead of `gh api`, so the runner needs no GitHub CLI.
- `CONTRIBUTING.md` (*Checks*): where the jobs run.

## Commits
- feat(ci): moved linux jobs to the self-hosted runner
- feat(changelog): updated changelogs

## Notes
- Until a runner with the default labels is online for the organization, the Linux jobs
  stay queued (GitHub cancels them after 24 hours); the Windows job still runs.
- Validated with `actionlint` (with shellcheck).
