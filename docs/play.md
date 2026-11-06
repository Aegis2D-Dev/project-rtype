# Play

Up to four players, each with their own `r-type_client`, join one `r-type_server` and fly
through the waves of Bydos together. This page tells how to start a game, the controls, what
the screen shows, and what happens when a player or the server goes away.

## Start a game

Start the server, then one client per player, on the same machine or on others of the
network (build first, see [Getting started](getting-started.md)):

```bash
./build/linux-debug/r-type_server
./build/linux-debug/r-type_client
```

The client opens on the **connect screen**: the server's address, its TCP port and the
player's name, filled with the command line's values (`127.0.0.1`, `4242`, `player` by
default). Type over them, then press Enter to join:

| Key | On the connect screen |
|---|---|
| Any character the field takes | Type it at the cursor of the selected field |
| Left, Right / Home, End | Move the cursor / to the start or the end |
| Backspace / Delete | Erase the character before / after the cursor |
| Tab, Down / Shift+Tab, Up | Select the next / previous row |
| Enter | Join the server; on a control row, change its key |
| Escape | Quit |

What is typed follows the keyboard's layout, punctuation included, so an address typed on
an AZERTY keyboard has its dots and colons. The address takes letters, digits, `.`, `:`,
`-`, `[` and `]`, the port digits, the name any printable character, accented letters
included, up to 16 bytes (an accented letter takes two); anything else is left out, and so
is it from `--host` and `--name`, which fill the fields as if typed. A text longer than its box scrolls to keep the cursor in view. A field the
server cannot accept is told before
joining (an empty address, a port above 65535, a name longer than 16 characters), and
what keeps the client out once it tries is told too: the server is full, the name is
taken, the server does not answer, or its UDP port is blocked.

Once logged in, the client shows the **rooms** of the server, refreshed every second: each
one with its name, its players and its size, waiting for players or playing. Each room is a
game of its own, of up to 4 players. Choose one and press Enter to go in it, or create one
on the last row: type its name, choose its size, and press Enter; it is listed at once,
ready to be joined:

| Key | On the rooms screen |
|---|---|
| Up / Down | Select a room, or the new room on the last row |
| Enter | Join the selected room, or create the new room |
| Letters, digits, spaces | Type the name of the new room, up to 16 characters |
| Backspace | Erase the last character of the name |
| Left / Right | One player less / more in the new room, 1 to 4 |
| Escape | Leave the server, back to the connect screen |

A full room is told before joining, and what the server refuses is told too: a room gone
meanwhile, or no room left on the server.

In a room, the screen lists its players, the player's own highlighted, each **ready** or not.
Press Enter when ready (again to take it back): the game starts for everyone once every player
of the room is ready, and the room's players fly together. Escape goes back to the rooms. A
room already playing takes a new player as soon as it is ready.

To skip the connect screen, as for scripts or several clients on one machine, pass the
server and `--join`:

```bash
./build/linux-debug/r-type_client --host 192.168.1.20 --port 4242 --name R9 --join
```

| Option | Meaning |
|---|---|
| `--host <address>` | Server: a numeric IPv4 or IPv6 address, or `localhost` |
| `--port <tcp port>` | TCP port of the server |
| `--name <name>` | Player name, 1 to 16 characters |
| `--join` | Join at once instead of waiting on the connect screen, and play in the room the server gives |
| `--settings <file>` | Settings file to use instead of the player's own (below) |
| `--frames <count>` | Quit after this many frames (tests, captures) |
| `--screenshot <png>` | Save the frame before the last one (after 300 frames by default) |

## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Move | Arrows | D-pad or left stick |
| Fire | Space | A (Cross) or right trigger |
| Leave the game | Escape | |
| Auto-fire on / off | F1 | Back |
| Flashes and shake on / off | F2 | |
| Color vision preview | F3 | |
| Smaller / larger text | F4 / F5 | |
| Lagometer on / off | F6 | |
| Music on / off | M (in game) | |

The movement and fire keys can be changed on the connect screen: select a control with
the arrows or Tab, press Enter, then the new key (Escape keeps the old one). A key already
used by another action is taken from it, and the screen says so. With auto-fire on, a
press of Fire starts firing and the next one stops it, so a player does not have to hold
the key.

The F-keys work on every screen; the keys of the game show at the top of the screen for
the first seconds of a game, and again after each change.

## Settings

Keys, auto-fire, text size, flashes and shake are saved at each change, in `settings.json`
in the player's settings folder (`~/.local/share/Aegis2D/RType/` on Linux,
`%APPDATA%\Aegis2D\RType\` on Windows, `~/Library/Application Support/Aegis2D/RType/` on
macOS), and read back at the next start. `--settings` uses another file, so that several
clients on one machine can each keep their own.

## What the screen shows

- **The ships**, each in its player's color, with its slot's marker (circle, triangle,
  square, diamond) and number (`P1` to `P4`) next to it, so the players tell each other
  apart with or without color; `YOU` marks the player's own ship.
- **The Bydos**, their missiles (red squares) and the players' missiles (in the color of
  the ship that fired).
- **Explosions** where an enemy or a ship is destroyed; the screen flashes red and shakes
  when the player's own ship is hit (off with F2).
- **The HUD**, at the bottom: for each slot, the player's name, lives and score, or `OUT`
  once out of lives.
- **Notices**, at the top, in the player's color: who joins the game, who leaves it and
  why (`R9 left the game`, `R9 lost the connection`, `R9 was removed from the game`).
- **`YOU ARE OUT`** once the player has no life left (the game goes on for the others),
  **`GAME OVER`** once every player is out.

Sounds play with the game: each shot, each explosion and each life lost, over the music.
A machine without an audio output plays silently.

## Smooth movement

The server sends the state of the world 20 times per second, and the client draws 60 frames
per second or more. Drawn where the last snapshot put them, the entities would jump every
third frame. The client draws them where they were 100 milliseconds ago, between the two
snapshots around that time, so they move smoothly; a late or lost snapshot is covered,
since the next one is already there. An entity the server just created shows once the
drawing reaches its time.

The delay is the same for every entity, the player's own ship included: what is drawn is
the world as it was, consistent between the ships, the missiles and the enemies.

## When a player or the server goes away

- **Escape** leaves the game: the client says goodbye to the server (`Logout`) and goes
  back to the connect screen, from which the player can join again.
- **A player who leaves**, closes the client or loses the connection is announced to the
  others, by name, and its slot in the HUD empties. A client that freezes is dropped by
  the server after 5 seconds of silence.
- **The server stops** (Ctrl+C, a crash) **or goes silent** for 5 seconds: the client
  shows the **connection lost** screen with the reason. Enter goes back to the connect
  screen, filled as before, to join again once the server is back.

While in game, the client sends the actions the player holds 60 times per second: these
inputs are also the heartbeat that tells the server the player is still there.

### Lagometer

F6 shows the **lagometer** at the top right of the game: seven small graphs of the last 10
seconds, one bar every 100 ms, newest on the right, each with its last value.

| Graph | What it shows | Yellow from | Red from |
|---|---|---|---|
| RTT | Smoothed round trip time to the server | 100 ms | 200 ms |
| Jitter | How irregularly the snapshots arrive | 10 ms | 30 ms |
| Loss | Share of the server's datagrams lost, over 2 s | 1 % | 5 % |
| Snapshots | Mean time between two snapshots (50 ms on a sound link) | 60 ms | 100 ms |
| In / Out | Bytes received and sent during the last second (blue) | | |
| Corrected | Times per second the server put the player's own ship elsewhere than the client predicted | 2 | 10 |

A graph keeps its bars below its top on a sound link (100 ms for the RTT, 10 % for the
loss...), and grows when a value goes above it. To see it react, play with a degraded
network: `sudo tools/netem.sh start` in the engine (see its *Test on a degraded network*
page) delays each direction of `lo` by 150 ms, so the RTT climbs above 300 ms and turns
red within a second, the loss turns yellow, and the bandwidth graphs flatten at 5 KB/s;
after `sudo tools/netem.sh stop`, the loss is back to 0 within 2 seconds and the RTT to
normal within a few.

## Accessibility

- **Text size**: F4 and F5 make every text smaller or larger, from 100 % to 200 %; the
  HUD fits its texts to the space it has.
- **Color vision**: F3 shows the game as a player with protanopia, deuteranopia,
  tritanopia or achromatopsia sees it, sprites included: the players stay apart by their
  markers, numbers and lightness.
- **Flashes and shake**: F2 turns them off, for players with photosensitive epilepsy or
  motion sickness; even when on, flashes never start more than three times a second.
- **Keys**: every movement and fire key can be changed, and auto-fire spares holding Fire.
