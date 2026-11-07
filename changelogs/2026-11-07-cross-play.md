# Cross-play Linux and Windows: why it works, the test procedure and the sessions

| | |
|---|---|
| Pull request | [#102](https://github.com/Aegis2D-Dev/project-rtype/pull/102) (opened 2026-11-07) |
| Branch | `test/ci-cross-play` → `develop` |
| Status | Open |
| Author | @Loic-ally |
| Ticket | [Aegis2D-Dev/aegis2d#389](https://github.com/Aegis2D-Dev/aegis2d/issues/389) test(ci): cross-play Linux and Windows |

## Summary
The documentation site gets a page on playing across systems, *Cross-play Linux and
Windows*: why a Linux server takes Windows clients and the other way round (the packets are
written field by field in little-endian, their exact bytes are tested on Linux and on
Windows by the CI, and only the server simulates), what is needed (the same version on both
machines, the ports to open in each system's firewall), the test procedure, played both
ways, with what each step of a game must show and `r-type_bot` for a machine without a
second screen, and a table of the sessions played. Part of the *Delivery 2: common part*
epic (Aegis2D-Dev/aegis2d#387).

## Added
- `docs/cross-play.md`, in the site's navigation after *Play*.

## Changed
- *Play*: a link to the new page before the connect screen.

## Commits
- feat(docs): documented the cross-play procedure and updated changelogs

## Notes
- Draft until a session is recorded: the ticket is done when a cross-play session is in the
  page's *Sessions* table, which needs a Linux and a Windows machine side by side. Play the
  procedure both ways, fill the table, then mark the pull request ready.
- No new test: the page relies on the byte-exact protocol tests that already run on Linux
  and Windows in *Code quality*.
- Verified with `mkdocs build --strict`. A documentation-only pull request: the
  *Documentation* workflow checks it.
