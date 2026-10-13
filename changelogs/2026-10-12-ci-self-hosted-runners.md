# CI on self-hosted runners

| | |
|---|---|
| Pull request | [#15](https://github.com/Aegis2D-Dev/project-rtype/pull/15) (opened 2026-10-12) |
| Branch | `ci/self-hosted-runners` → `develop` |
| Status | Merged on 2026-10-12 (`1122f5c`) |
| Author | @vmbr068 |

## Summary
The Linux jobs leave the GitHub-hosted `ubuntu-24.04` runners for the organization's
self-hosted NixOS runners on `cust-aegis2d-01` and `cust-aegis2d-02` (configured in `Aegis2D-Dev/infra-config`),
so they no longer spend GitHub Actions minutes. They run inside the engine's `aegis2d-ci`
image (Aegis2D-Dev/aegis2d#211), which has the whole toolchain preinstalled. The Windows
job moves to a self-hosted Windows runner. Supersedes #14.

## Changed
- *Code quality*, *Documentation* and *Request Epitech mirror*: Linux jobs use
  `runs-on: [self-hosted, nixos]` and the `ghcr.io/aegis2d-dev/aegis2d-ci:ubuntu-24.04`
  container (`node:24` for the Cloudflare deploy), with `packages: read` to pull it.
- The `apt-get` and `actions/setup-python` steps of the Linux jobs are gone; the
  `pip install -r` steps stay and only check the preinstalled versions.
- `CPM_SOURCE_CACHE` is set from `$GITHUB_WORKSPACE` in container jobs, because
  `github.workspace` is the host path there; the Windows job keeps its own.
- *Code quality*: the Windows job uses `runs-on: [self-hosted, windows]`.
  Its git credentials for the engine go to a `gitconfig` in `$RUNNER_TEMP`
  (`GIT_CONFIG_GLOBAL`), because that runner keeps its disk between jobs.

## Commits
- ci: run Linux jobs on self-hosted NixOS runners in Ubuntu containers
- feat(ci): fixed git ownership in container jobs
- feat(ci): used prebuilt ci image
- feat(changelog): updated changelogs
- feat(ci): moved the Windows job to the self-hosted runner

## Notes
- Each container mounts `/nix/store` read-only: the NixOS runner's Node.js, which runs the
  actions, links into it.
- The image belongs to the engine repository; this repository reads it through the
  package's *Manage Actions access* setting.
- The Windows runner, `cust-aegis2d-win-01`, is a persistent VM with Git, Python and the
  Visual Studio Build Tools (ClangCL); it runs one job at a time.
