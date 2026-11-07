# Accessibility of the lobby, the chat and the lagometer: gamepad, text size, above the HUD

| | |
|---|---|
| Pull request | [#103](https://github.com/Aegis2D-Dev/project-rtype/pull/103) (opened 2026-11-07) |
| Branch | `docs/a11y-new-screens` → `develop` |
| Status | Open |
| Author | @Loic-ally |
| Ticket | [Aegis2D-Dev/aegis2d#390](https://github.com/Aegis2D-Dev/aegis2d/issues/390) docs(a11y): accessibility of the new screens |

## Summary
The screens added for the second delivery follow the accessibility rules of the game. The
lobby takes a gamepad: on the rooms screen, in a room, while joining and on the connection
lost screen, the D-pad or the left stick moves as the arrows, A (Cross) is Enter and B
(Circle) is Escape, so a player on a gamepad joins, readies and leaves a game without the
keyboard once connected. The chat line follows the text size and the color vision preview,
and stays above the HUD: in game it was drawn under the HUD's bar, hidden. The lagometer's
labels follow the text size too, its panel widening to the longest one. The engine's
accessibility page lists these screens and their options (Aegis2D-Dev/aegis2d#484).

## Added
- `client/ui/menu_keys.hpp` and `.cpp`: `menu_keys()`, the keys pressed in the frame with the
  gamepad's D-pad, left stick, South and East as the arrows, Enter and Escape.
- Tests (2 new test cases): the D-pad, the stick, A and B give their keys, the fire and option
  buttons none; the keyboard's keys come first, and a key and its button count once.

## Changed
- `client_app.cpp`: the rooms, room, joining and connection lost screens read `menu_keys()`;
  the chat line's tree takes the text size, from its bottom-left corner.
- `menu_screens`: the chat line moved above the HUD (`chat_box` at y 610, 600 wide, so that it
  fits at 200%), drawn over the HUD and the lagometer, its text, placeholder, cursor and
  outline in the color vision preview; the messages sit above the line at its size.
- `lagometer_view.cpp`: the labels at the text size, the panel as wide as the longest.
- *Play*: the gamepad in the lobby, and the chat and lagometer in *Accessibility*.

## Commits
- `aaafac6` feat(client): added gamepad to the lobby and scaled chat and lagometer
- `dc7b9f8` feat(tests): added the lobby keys from a gamepad
- feat(docs): documented the accessibility of the new screens and updated changelogs

## Notes
- The engine's part, the accessibility page, is Aegis2D-Dev/aegis2d#484, which closes the
  ticket; this one refers to it.
- The connect screen stays on the keyboard: its fields are typed. `--join` skips it.
- Seen on macOS (`macos` build) with a server and a client joined with `--join`, at 100 % and
  200 % text, the lagometer and the chat line forced open for the screenshot: both fit the
  screen, the chat line above the HUD.
- Verified on macOS (AppleClang 21): build without warnings; 325 of 327 tests pass (ctest;
  the two flood tests of `server_test.cpp` bind `127.0.0.2`, which macOS lacks without an
  alias, on `develop` as well); clang-format; clang-tidy reports nothing in the changed code;
  `mkdocs build --strict`.
