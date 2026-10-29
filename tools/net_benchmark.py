#!/usr/bin/env python3
"""Network benchmark: a server and bots under each netem profile, measured into one CSV.

    python3 tools/net_benchmark.py run --build build/linux-release --name "Arch, Ryzen 7"
    python3 tools/net_benchmark.py run --build build/server --name ci --profiles lan

`run` starts r-type_server and, for each profile (LAN, 50 ms, 150 ms + 2 % loss, 300 ms
+ 5 % loss), degrades the server's ports on the loopback with the engine's tools/netem.sh,
runs `r-type_bot --count N --seconds S --stats`, and writes one line per profile, the mean
of the bots, to docs/network-benchmark/<name>.csv (the machine's name, cleaned into a file
name). The netem delay is added to each packet, so on lo to each direction: the RTT grows
by twice the delay. netem needs root: the script runs it with sudo unless it is root, and
the LAN profile needs nothing. Only the standard library is used (Python 3.10 or newer).
"""

from __future__ import annotations

import argparse
import csv
import os
import platform
import re
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RESULTS = ROOT / "docs" / "network-benchmark"
# name: (delay added to each packet in ms, loss in %)
PROFILES = {
    "lan": (0, 0.0),
    "50ms": (50, 0.0),
    "150ms-2%": (150, 2.0),
    "300ms-5%": (300, 5.0),
}
FIELDS = ("machine", "os", "profile", "delay_ms", "loss_set_percent", "bots", "seconds",
          "bytes_in_per_s", "bytes_out_per_s", "rtt_ms", "rtt_jitter_ms", "loss_percent",
          "snapshot_jitter_ms", "corrections")
# Measured by the bots, averaged over them: --stats columns.
MEANS = ("bytes_in_per_s", "bytes_out_per_s", "rtt_ms", "rtt_jitter_ms", "loss_percent",
         "snapshot_jitter_ms")


def slug(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-") or "machine"


def executable(build: Path, name: str) -> Path:
    for candidate in (build / name, build / f"{name}.exe"):
        if candidate.is_file():
            return candidate
    sys.exit(f"{name} not found in {build}: build it first")


def engine_netem(build: Path) -> Path | None:
    """The engine's tools/netem.sh, from the sources the build tree was configured with."""
    cache = build / "CMakeCache.txt"
    if cache.is_file():
        found = re.search(r"^aegis2d_SOURCE_DIR:\w+=(.+)$", cache.read_text(), re.MULTILINE)
        if found and (Path(found.group(1)) / "tools" / "netem.sh").is_file():
            return Path(found.group(1)) / "tools" / "netem.sh"
    return None


def netem(script: Path, action: str, options: list[str], dry_run: bool) -> None:
    command = [str(script), action, *options]
    if dry_run:
        command.append("--dry-run")
    elif os.geteuid() != 0:
        command.insert(0, "sudo")
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL if not dry_run else None)


def start_server(exe: Path, tcp: int, udp: int, log: Path) -> subprocess.Popen[bytes]:
    """Starts the server, its output in @p log, and waits until it listens."""
    with log.open("wb") as output:
        server = subprocess.Popen([str(exe), "--tcp-port", str(tcp), "--udp-port", str(udp)],
                                  stdout=output, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if "listening:" in log.read_text(errors="replace"):
            return server
        if server.poll() is not None:
            break
        time.sleep(0.1)
    stop_server(server)
    sys.exit(f"{exe.name} did not listen:\n{log.read_text(errors='replace')}")


def stop_server(server: subprocess.Popen[bytes]) -> None:
    # Ctrl+C stops the server cleanly; Windows cannot send it to another process.
    if os.name == "nt":
        server.terminate()
    else:
        server.send_signal(signal.SIGINT)
    try:
        server.wait(timeout=10)
    except subprocess.TimeoutExpired:
        server.kill()


def measure(args: argparse.Namespace, profile: str, script: Path | None) -> dict[str, str]:
    delay, loss = PROFILES[profile]
    degraded = delay > 0 or loss > 0
    if degraded and script is None:
        sys.exit("netem.sh not found: give --netem <engine>/tools/netem.sh")
    ports = f"{args.tcp_port},{args.udp_port}"
    netem_options = ["--dev", args.dev, "--ports", ports, "--delay", f"{delay}ms",
                     "--loss", f"{loss}%", "--duplicate", "0%", "--rate", "1gbit"]
    folder = Path(tempfile.mkdtemp(prefix="net-benchmark-"))
    server = start_server(executable(args.build, "r-type_server"), args.tcp_port, args.udp_port,
                          folder / "server.log")
    try:
        if degraded and script is not None:
            netem(script, "start", netem_options, args.dry_run)
        stats = folder / "bots.csv"
        done = subprocess.run(
            [str(executable(args.build, "r-type_bot")), "--port", str(args.tcp_port),
             "--count", str(args.bots), "--seconds", str(args.seconds), "--seed", "1",
             "--stats", str(stats)],
            capture_output=True, text=True, check=False)
        if done.returncode != 0 or not stats.is_file():
            sys.exit(f"r-type_bot failed ({done.returncode}):\n{done.stdout}{done.stderr}")
        with stats.open(newline="") as file:
            bots = list(csv.DictReader(file))
    finally:
        if degraded and script is not None:
            netem(script, "stop", ["--dev", args.dev], args.dry_run)
        stop_server(server)
        shutil.rmtree(folder, ignore_errors=True)
    if not bots:
        sys.exit(f"{profile}: no bot played")
    line = {
        "profile": profile,
        "delay_ms": str(delay),
        "loss_set_percent": f"{loss:g}",
        "bots": str(len(bots)),
        "seconds": f"{statistics.fmean(float(bot['seconds']) for bot in bots):.1f}",
        # Not measured yet: the client counts its corrections with prediction (#294).
        "corrections": "",
    }
    for field in MEANS:
        line[field] = f"{statistics.fmean(float(bot[field]) for bot in bots):.2f}"
    return line


def run(args: argparse.Namespace) -> None:
    profiles = [name.strip() for name in args.profiles.split(",") if name.strip()]
    unknown = [name for name in profiles if name not in PROFILES]
    if unknown or not profiles:
        sys.exit(f"unknown profile {', '.join(unknown)}: choose among {', '.join(PROFILES)}")
    script = args.netem or engine_netem(args.build)
    os_name = args.os or f"{platform.system()} {platform.release()}"
    lines = []
    for profile in profiles:
        print(f"{profile}: {args.bots} bots for {args.seconds} s...", flush=True)
        line = {"machine": args.name, "os": os_name, **measure(args, profile, script)}
        print(f"  {line['bytes_in_per_s']} B/s in, {line['bytes_out_per_s']} B/s out, "
              f"RTT {line['rtt_ms']} ms, loss {line['loss_percent']} %")
        lines.append(line)
        time.sleep(1)  # the ports of the server free again
    if args.dry_run and args.output is None:
        print("dry run: the network was not degraded, nothing written (see --output)")
        return
    RESULTS.mkdir(parents=True, exist_ok=True)
    target = args.output or RESULTS / f"{slug(args.name)}.csv"
    with target.open("w", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(lines)
    print(f"{target}: {len(lines)} profile(s)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    command = commands.add_parser("run", help="measure this machine")
    command.add_argument("--build", type=Path, required=True,
                         help="build tree with r-type_server and r-type_bot")
    command.add_argument("--name", required=True, help='machine, e.g. "Arch, Ryzen 7"')
    command.add_argument("--os", help="system, guessed when not given")
    command.add_argument("--profiles", default=",".join(PROFILES),
                         # argparse formats the help with %: the profiles' % are doubled.
                         help=f"comma-separated, among {', '.join(PROFILES)} (all)"
                         .replace("%", "%%"))
    command.add_argument("--bots", type=int, default=4, help="bots per game (4)")
    command.add_argument("--seconds", type=int, default=30, help="play time per profile (30)")
    command.add_argument("--netem", type=Path,
                         help="the engine's tools/netem.sh, found from --build by default")
    command.add_argument("--dev", default="lo", help="interface netem degrades (lo)")
    command.add_argument("--tcp-port", type=int, default=4342, help="server TCP port (4342)")
    command.add_argument("--udp-port", type=int, default=4343, help="server UDP port (4343)")
    command.add_argument("--output", type=Path, help="CSV to write instead of docs/...")
    command.add_argument("--dry-run", action="store_true",
                         help="print the netem commands instead of running them, and write "
                         "nothing unless --output is given")
    command.set_defaults(handler=run)
    args = parser.parse_args()
    args.handler(args)


if __name__ == "__main__":
    main()
