# README, documentation site and API reference

| | |
|---|---|
| Pull request | [#12](https://github.com/Aegis2D-Dev/project-rtype/pull/12) (opened 2026-10-12) |
| Branch | `docs/documentation` → `develop` |
| Status | Merged on 2026-10-12 (`83d0bed`) |
| Author | @Loic-ally |

## Summary
The game gets the documentation the subject asks for, built like the engine's: a README
(purpose, binaries, requirements, build, presets, license, team), a MkDocs Material site
with the getting started guide, the architecture, the gameplay and how-to guides, and a
Doxygen API reference of the game's headers. Both are built on every pull request that
touches them and deployed to [rtype-docs.pages.dev](https://rtype-docs.pages.dev) from
`develop`.

## Added
- `README.md` and `LICENSE` (MIT, like the engine).
- `mkdocs.yml` and `docs/requirements.txt` (the engine's theme and versions).
- `docs/index.md`, `docs/getting-started.md` (requirements, access to the private engine,
  presets, options, running, building the docs), `docs/architecture.md` (layers and
  server/client diagrams, the code, why `rtype_game` is static, delivery),
  `docs/gameplay.md` (`ServerGame`, the nine systems of a tick, collision layers,
  entities, tuning values), `docs/how-to.md` (change a value, add an enemy, add a system,
  write a test, update the docs).
- `docs/Doxyfile` and `docs/api-mainpage.md`: API reference of `game/include/`, warnings
  as errors.
- `.github/workflows/docs.yml`: *Documentation* workflow, the engine's: `mkdocs build
  --strict`, Doxygen into `site/api`, the site attached to the run, then a Cloudflare
  Pages deploy (`rtype-docs`) on pushes to `develop`.

## Changed
- `CONTRIBUTING.md`: *Documentation* section (preview, CI, one-time Cloudflare setup,
  Doxygen comments) and the *Documentation* workflow in the required checks.
- `.gitignore`: `/site/` and `/.wrangler/`.

## Commits
- feat(docs): added readme, license and documentation site
- feat(ci): added documentation workflow
- feat(changelog): updated changelogs

## Notes
- Deploying needs a one-time setup by an admin: create the `rtype-docs` Pages project and
  add the `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID` secrets to this repository
  (see `CONTRIBUTING.md`). Pull requests only build the site, without secrets.
- The workflow does not fetch the engine: Doxygen reads only the game's headers, and links
  to the engine's API reference for engine types.
- Checked locally: `mkdocs build --strict`, `doxygen docs/Doxyfile` (Doxygen 1.9.8, as on
  Ubuntu 24.04), `actionlint`, and the test of the how-to guide compiled and run.
- Made from `docs/contributing` (#11, not merged yet).
