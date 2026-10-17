# Playable client: connect screen, interpolation, lost connection, player notices

| | |
|---|---|
| Pull request | [#44](https://github.com/Aegis2D-Dev/project-rtype/pull/44) (opened 2026-10-17) |
| Branch | `feat/client-game` → `develop` |
| Status | Merged on 2026-10-17 (`329ca0e`) |
| Author | @Loic-ally |
| Ticket | Aegis2D-Dev/aegis2d#123 connect screen, #127 interpolate remote entities, #128 heartbeat and connection-lost screen, #129 disconnected-player notice |

## Summary
`r-type_client` becomes the game: a window with a connect screen, then the game drawn from
the server's snapshots and events, with sprites, explosions, sounds, a HUD per player and
the engine's accessibility options, until the player leaves or the connection is lost. It
brings into the game what the engine's example scenes show one at a time (connect,
players, ship, starfield, HUD, effects, sound, key bindings, auto-fire). Up to four
clients play together on one server. Ends the client networking epic
(Aegis2D-Dev/aegis2d#122).

## Added
- Connect screen (#123): server, port and name fields, filled from the command line,
  checked before joining (`ConnectForm`, `client/network/`), then given to
  `ClientSession`; the ship's keys listed and changed in place (Enter, then the key);
  joining, failure and cancel messages.
- Interpolation (#127): `Trail` keeps the last positions of each entity, `interpolate()`
  draws it at any tick between them, `ClientWorld::render_tick()` estimates the server's
  tick from the snapshots received and draws the world 100 ms in the past
  (`interpolation_delay`), so entities move smoothly at any frame rate with 20 snapshots a
  second.
- Connection-lost screen (#128): when the server stops, closes the connection or goes
  silent for 5 seconds, the reason shows; Enter goes back to the connect screen, Escape
  quits. The inputs sent every tick are the heartbeat.
- Player notices (#129): `PlayerNotices` turns `PlayerJoined` and `PlayerLeft` into
  "<name> joined the game", "<name> left the game", "<name> lost the connection" or
  "<name> was removed from the game", shown 5 seconds to the players who stay, in the
  player's color; the players already there when a client joins are not announced.
- `ClientWorld::take_events()`: the events applied, for the screen to show.
- The game screen (`GameView`): parallax starfield; ships animated and tinted with their
  player's color, with the slot's marker and number; Bydos animated; missiles in their
  shooter's color; explosions on kills and lost lives; red flash and shake when the
  player's ship is hit; HUD with each player's name, lives and score; `YOU ARE OUT` and
  `GAME OVER`.
- Sounds (`GameSounds`): shots, explosions and lost lives played from the server's events
  through the engine's `EventSounds`, and looping music (M); silent without an audio
  output.
- Settings (`Preferences`): keys, auto-fire (F1), flashes and shake (F2), text size
  (F4 / F5) saved in the player's settings folder or `--settings`; color vision preview
  (F3), sprites included.
- Options `--join`, `--settings`, `--frames` and `--screenshot`; `parse_port()` public.
- Assets: `assets/sprites/` (ship, Bydos, explosion) and `assets/sounds/` (shot,
  explosion, death, music), from the engine's examples, copied next to `r-type_client`.
- Tests: interpolation between snapshots at 20 Hz, the server clock against late
  datagrams, the events handed out once, the connect form, the player notices, the new
  options.
- *Play*: a new documentation page on starting a game, the controls, the screens, the
  settings, the smooth movement and the accessibility options.

## Changed
- `ClientSession` times each snapshot for the server clock of the interpolation.
- `client/main.cpp` runs the client (`ClientApp`) instead of joining and leaving.
- *Getting started*, *Architecture*, home page and README: the client is playable; the
  client's code and assets.
- Changelog: #42 (`a2e0dbb`) marked as merged, with its commits.

## Commits
- `27a4d91` feat(client): played the game from the server's world
- `7dbb0a8` feat(client): tested interpolation, connect form and player notices
- `a1bc28a` feat(client): documented how to play
- `1cac58f` feat(gitignore): added gitignore to skip graphify output

## Notes
- Verified: 144 tests, clang-format and clang-tidy 22, Doxygen, `mkdocs build --strict`;
  a server with four clients under Xvfb: each player told apart, a frozen client announced
  as "lost the connection" on the three others, a stopped server showing the
  connection-lost screen, text at 200 %.
- Text typed on the connect screen follows the US positions for punctuation; letters and
  digits work on any layout, and the keypad's `.` types a dot.
- The player's own ship is drawn with the same delay as the others; client-side
  prediction can come later if the delay is felt.
