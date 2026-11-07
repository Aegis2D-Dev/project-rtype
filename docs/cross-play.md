# Cross-play Linux and Windows

A Linux `r-type_server` takes Windows clients, and a Windows server Linux ones: client and
server only share the bytes of the protocol, and nothing in them depends on the system that
wrote them. This page tells why, how to check it on two machines, and records the sessions
played that way.

## Why it works

- Every packet is written and read field by field with the engine's `net::BinaryWriter` and
  `net::BinaryReader` (little-endian, fixed sizes, no structure copied as it lies in
  memory), and the snapshots bit by bit with `net::BitWriter` and `net::BitReader`.
- The tests check the exact bytes of the packets (`login_test.cpp`, `snapshot_test.cpp`,
  `events_test.cpp`, `input_test.cpp`, `chat_test.cpp`, `scores_test.cpp`...). The *Code
  quality* workflow runs them on Linux (Clang) and on Windows (clang-cl): both write the
  same bytes, so each reads the other's.
- The simulation runs on the server only; the client predicts its own ship with the same
  `rtype::step_ship()` and is corrected by the server, so a difference of floating point
  between the two systems shows as a correction, never as a desynchronized game.

## What is needed

- One Linux machine and one Windows machine on the same network, which can reach each
  other (`ping`).
- The same version of the game on both: the packages of the same
  [release](https://github.com/Aegis2D-Dev/project-rtype/releases), or both built from the
  same commit (see [Getting started](getting-started.md)). `--version` prints it.
- The server's ports open on its machine: TCP 4242 (login, rooms, chat) and UDP 4243 (the
  game), or the ones given with `--tcp-port` and `--udp-port`.
    - Linux: `sudo ufw allow 4242/tcp && sudo ufw allow 4243/udp` with ufw, or
      `sudo firewall-cmd --add-port=4242/tcp --add-port=4243/udp` with firewalld.
    - Windows: accept the *Windows Defender Firewall* prompt on the first start of
      `r-type_server.exe` for private networks, or add the rule once from an
      administrator PowerShell:
      `New-NetFirewallRule -DisplayName "R-Type" -Direction Inbound -Protocol UDP -LocalPort 4243 -Action Allow`
      (and the same with `-Protocol TCP -LocalPort 4242`).

## Test procedure

Play both ways: a Linux server with a Windows client, then a Windows server with a Linux
client. For each, with two clients if possible (one on each machine):

1. On the server's machine, note its address (`ip -4 addr` on Linux, `ipconfig` on
   Windows), then start the server with its log kept:

    ```bash
    bin/r-type_server 2>&1 | tee server.log           # Linux
    ```

    ```powershell
    bin\r-type_server.exe 2>&1 | Tee-Object server.log   # Windows
    ```

2. On the other machine, start the client with the server's address:
   `bin/r-type_client --host <address> --name WIN1` (or `LINUX1`), then Enter on the
   connect screen.
3. Check each step of a game, and note the ones that fail:

    | Step | Expected |
    |---|---|
    | Login | The rooms screen; the server logs the player's login |
    | Rooms | A room created on one machine is listed on the other within a second |
    | Chat | T, a message, Enter: it shows on every client of the room |
    | Ready | Enter on both clients: the game starts on both at once |
    | Game | Ships, enemies, shots and explosions move the same on both screens; F6 shows the lagometer: RTT green, loss at 0 %, *Corrected* at 0 most of the time |
    | Score | Killing an enemy raises the score on both HUDs |
    | Leave | Escape on one client: the other is told who left, its slot empties |
    | Stop | Ctrl+C on the server: every client shows the *connection lost* screen with `Shutdown` |

4. Without a second screen, `r-type_bot` (built from the sources, see
   [Getting started](getting-started.md)) plays from the other system:
   `r-type_bot --host <address> --count 2 --seconds 60 --stats bots.csv`. Every bot must
   join, play the 60 seconds and leave with its reason, and `bots.csv` hold one line per
   bot.
5. Record the session below: the date, both systems, the version, the result, and any
   step that failed with its log.

## Sessions

| Date | Server | Clients | Version | Result | Played by |
|---|---|---|---|---|---|
| | | | | No session recorded yet | |
