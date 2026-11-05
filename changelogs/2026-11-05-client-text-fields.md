# Connect screen typed in the engine's text fields

| | |
|---|---|
| Pull request | [#82](https://github.com/Aegis2D-Dev/project-rtype/pull/82) (opened 2026-11-05) |
| Branch | `feat/client-text-fields` → `develop` |
| Status | Open |
| Author | @Neptnium |
| Ticket | [Aegis2D-Dev/aegis2d#345](https://github.com/Aegis2D-Dev/aegis2d/issues/345) feat(ui): text fields |

## Summary
The connect screen's server, port and name are now the engine's text fields
(Aegis2D-Dev/aegis2d#445), typed through the platform's text input (#443): what the player
types follows the keyboard layout, so an address typed on an AZERTY keyboard has its dots
and colons, which the client's own key table got wrong; the cursor moves with the arrows,
Home and End, Backspace and Delete erase at it, and an address longer than its box scrolls
instead of running past it. The fields keep the connect form's rules (what each field
takes, its length, the checks before joining). This is the game part of the ticket, whose
bar is the R-Type connect screen using the text fields; the engine part is merged.

## Added
- `client/ui`, `rtype_client_ui`: a static library built with the client, so the tests
  reach what the screen draws without a window. `ConnectFields` holds the three fields as
  `ui::TextField` with the `ConnectForm` they edit: `update(typed, keyboard)` gives the
  focused field the frame's typed text, without the characters it does not take, and the
  editing keys (Left and Right, Home and End, Backspace and Delete); `focus()` selects a
  field, or none on a control row, a field gained again editing from its end; `draw()` puts
  a field in its row's box; `form()` is what joining submits. The settings of the command
  line go through the form's rules first, so the fields show exactly what joining sends (a
  `--name` too long or with characters the name does not take is cut as if typed), and
  `--join` joins with them.
- `ConnectForm::accepts(field, character)`, `max_length(field)` and `set_text(field, text)`:
  the rules of each field, told and kept for text edited elsewhere. Names take UTF-8, as the
  login does (`valid_player_name()`): accented letters can be typed now, and a name is cut to
  its 16 bytes between characters, never inside one; `type()` keeps to ASCII characters.
- Tests: `tests/connect_fields_test.cpp` (10 test cases, built with the client): the fields
  filled from the settings, nothing typed with no field selected; an address with its
  layout's dots and colons, `[::1]`, a space, a slash and an accent left out; each field's
  characters and length, accented letters in the name up to 16 bytes; a full name refusing
  what is typed at its start, its cursor kept; an accented name typed and joined with; editing at the cursor with the arrows, Home, End, Backspace and
  Delete; the typed fields joined with, and a wrong one refused; a 45-character address
  drawn inside its box, the cursor only in the field being edited; a field gained again
  edited from its end; command line settings shown and submitted as one text. In
  `tests/connect_form_test.cpp` (1 more): `accepts()`, `max_length()` and `set_text()`,
  UTF-8 names cut between characters, and a lone byte of one refused by `type()`.

## Changed
- `ClientApp`: the fields are a `ConnectFields`; `--join` joins with what they hold, so a
  rejoin from the screen is the same (a `--name` the login refuses is told on the screen); the window's text input is on while a
  field is selected on the connect screen, off elsewhere (game keys never type); the typed
  text is applied before the keys that change rows, so a character typed just before Tab
  stays in its field and one typed just before Enter is joined with.
- `draw_connect()`: the fields drawn by their text fields in the rows' boxes, the one
  being edited outlined with its cursor (the blinking `_` and `ConnectView::seconds` are
  gone); the control rows are drawn as before.
- Removed: `typed_character`, the client's own table of the characters of the keys.
- Documentation: the connect screen's keys in *Play*; `client/ui` in the architecture's
  tree.

## Commits
- `b70b57d` feat(client): gave the connect form accepts, max_length and set_text
- `063beac` feat(client): typed the connect screen's fields in the engine's text fields
- `c4393fb` feat(tests): added the connect screen's text field tests
- `0eb969e` feat(client): kept the fields as the form sends them and their cursors at the end
- `ed1fb29` feat(client): applied the typed text before the keys that change rows
- `e749e62` feat(tests): added refocus and command line settings tests
- `df87d44` feat(client): took UTF-8 names in the connect form, cut between characters
- `c7f5401` feat(client): joined with the fields as shown when given --join
- `87c4043` feat(tests): added accented name and UTF-8 cut tests
- `b69c390` feat(client): refused typing past a name's 16 bytes instead of cutting its end
- `e688385` feat(tests): added a full name refusing typing at its start
- feat(docs): documented the connect screen's text fields and updated changelogs

## Notes
- Closes Aegis2D-Dev/aegis2d#345, whose engine part (`ui::TextField`) is
  Aegis2D-Dev/aegis2d#445; the game fetches the engine's `develop`, which has it.
- A review by independent agents found, before this was pushed: a field gained again kept
  its old cursor, settings of the command line could be shown otherwise than submitted,
  and a character typed just before Tab or Enter in the same frame went to the next field
  or was lost; all fixed here, the first two with tests (the third lives in `ClientApp`,
  which the tests do not build). A second round found that cleaning the settings dropped
  the accents of a `--name` the login takes, and that `--join` still joined with the raw
  settings: names take UTF-8 now, and `--join` joins with the fields. A third round found
  that the text field counts characters and the form bytes, so typing in a full name of
  accented letters cut its last letter: what does not fit in the bytes left is now refused
  before it is typed.
- On screen, in the macOS build: `r-type_client --host 2001:db8:85a3::8a2e:370:7334`
  captured at its 60th frame shows the three fields in their boxes, the name outlined with
  its cursor and the 28-character address inside its box. A field not being edited shows
  the end of a text longer than its box, where its cursor is.
- The server preset (no client) still builds `rtype_tests`, without
  `connect_fields_test.cpp`; its connect form tests pass.
- Verified on macOS (AppleClang 21): build without warnings, the changed files also with
  `-Wmissing-designated-field-initializers -Werror` as on Linux and Windows; 250 of 252
  tests pass (the two flood tests bind a UDP socket to `127.0.0.2`, which macOS does not
  route without an alias; they pass on Linux); `tools/format.sh`; `tools/lint.sh` clean on
  the changed files (the `bugprone-exception-escape` reports of macOS's libc++ elsewhere,
  `ConnectForm::erase()` included, are on `develop` too); `mkdocs build --strict`; Doxygen
  without warnings.
