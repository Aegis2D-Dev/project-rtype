#!/usr/bin/env python3
"""Network benchmark: a server and bots under each netem profile, measured into one CSV.

    python3 tools/net_benchmark.py run --build build/linux-release --name "Arch, Ryzen 7"
    python3 tools/net_benchmark.py run --build build/server --name ci --profiles lan
    python3 tools/net_benchmark.py report

`run` starts r-type_server and, for each profile (LAN, 50 ms, 150 ms + 2 % loss, 300 ms
+ 5 % loss), degrades the server's ports on the loopback with the engine's tools/netem.sh,
runs `r-type_bot --count N --seconds S --stats`, and writes one line per profile, the mean
of the bots, to docs/network-benchmark/<name>.csv (the machine's name, cleaned into a file
name). The netem delay is added to each packet, so on lo to each direction: the RTT grows
by twice the delay. netem needs root: the script runs it with sudo unless it is root, and
the LAN profile needs nothing. `--label` names the state of the code measured
(`baseline`, then each optimization): a run replaces the lines of its label and keeps the
others. `report` reads every CSV of docs/network-benchmark/ and rewrites the results of
docs/network-benchmark.md: per machine, a table comparing each label with the first and
two charts (docs/images/network-benchmark-<name>-<figure>.svg). Only the standard library
is used (Python 3.10 or newer).
"""

from __future__ import annotations

import argparse
import csv
import html
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
PAGE = ROOT / "docs" / "network-benchmark.md"
IMAGES = ROOT / "docs" / "images"
BEGIN = "<!-- results:begin -->"
END = "<!-- results:end -->"
# name: (delay added to each packet in ms, loss in %)
PROFILES = {
    "lan": (0, 0.0),
    "50ms": (50, 0.0),
    "150ms-2%": (150, 2.0),
    "300ms-5%": (300, 5.0),
}
FIELDS = ("machine", "os", "label", "profile", "delay_ms", "loss_set_percent", "bots", "seconds",
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


def start_server(exe: Path, tcp: int, udp: int, log: Path,
                 compress: int | None = None) -> subprocess.Popen[bytes]:
    """Starts the server, its output in @p log, and waits until it listens."""
    command = [str(exe), "--tcp-port", str(tcp), "--udp-port", str(udp)]
    if compress is not None:
        command += ["--compress", str(compress)]
    with log.open("wb") as output:
        server = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT)
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
                          folder / "server.log", args.compress)
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
        line = {"machine": args.name, "os": os_name, "label": args.label,
                **measure(args, profile, script)}
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
        writer.writerows(merge(read_lines(target), lines))
    print(f"{target}: {len(lines)} profile(s) of {args.label}")


def merge(old: list[dict[str, str]], new: list[dict[str, str]]) -> list[dict[str, str]]:
    """The new lines replace the old ones of their label and profile; the others stay."""
    measured = {(line["label"], line["profile"]) for line in new}
    return [*(line for line in old if (line["label"], line["profile"]) not in measured), *new]


def read_lines(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        return []
    with path.open(newline="") as file:
        return [{field: line.get(field) or "" for field in FIELDS} for line in csv.DictReader(file)]


def bar_chart(title: str, axis: str, values: dict[tuple[str, str], float],
              labels: list[str], profiles: list[str]) -> str:
    """Bars of one figure: a group per profile, a bar per label."""
    width, height = 640, 360
    left, right, top, bottom = 72, 24, 64, 48
    highest = max([*values.values(), 1.0]) * 1.15
    step = 10 ** len(str(int(highest / 5))) / 10
    tick = max(step * round(highest / 5 / step), step)
    group = (width - left - right) / max(len(profiles), 1)
    bar = min(group * 0.8 / max(len(labels), 1), 48.0)

    def y_of(value: float) -> float:
        return top + (height - top - bottom) * (1 - value / highest)

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
        f'font-family="sans-serif" font-size="12" role="img" aria-label="{html.escape(title)}">',
        f'<rect width="{width}" height="{height}" fill="#ffffff"/>',
        f'<text x="{left}" y="22" font-size="14" font-weight="bold">{html.escape(title)}</text>',
    ]
    value = 0.0
    while value <= highest:
        y = y_of(value)
        parts.append(f'<line x1="{left}" x2="{width - right}" y1="{y:.1f}" y2="{y:.1f}" '
                     'stroke="#e0e0e0"/>')
        parts.append(f'<text x="{left - 8}" y="{y + 4:.1f}" text-anchor="end">{value:g}</text>')
        value += tick
    parts.append(f'<text x="16" y="{(top + height - bottom) / 2}" text-anchor="middle" '
                 f'transform="rotate(-90 16 {(top + height - bottom) / 2})">'
                 f'{html.escape(axis)}</text>')
    # Colors that stay apart with color blindness (Okabe-Ito), named in the legend.
    colors = ("#0072B2", "#D55E00", "#009E73", "#CC79A7", "#E69F00", "#56B4E9")
    for index, label in enumerate(labels):
        color = colors[index % len(colors)]
        x = left + index * 150
        parts.append(f'<rect x="{x}" y="34" width="12" height="12" fill="{color}"/>')
        parts.append(f'<text x="{x + 18}" y="44">{html.escape(label)}</text>')
    for slot, profile in enumerate(profiles):
        start = left + slot * group + (group - bar * len(labels)) / 2
        parts.append(f'<text x="{left + (slot + 0.5) * group:.1f}" y="{height - bottom + 18}" '
                     f'text-anchor="middle">{html.escape(profile)}</text>')
        for index, label in enumerate(labels):
            if (label, profile) not in values:
                continue
            y = y_of(values[(label, profile)])
            parts.append(f'<rect x="{start + index * bar:.1f}" y="{y:.1f}" width="{bar:.1f}" '
                         f'height="{height - bottom - y:.1f}" '
                         f'fill="{colors[index % len(colors)]}"/>')
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def change(value: str, first: str) -> str:
    """A value, with how far it is from the first label's when it is not the first."""
    if not value or not first or float(first) == 0.0 or value == first:
        return value or "-"
    return f"{value} ({(float(value) / float(first) - 1) * 100:+.0f} %)"


def report(args: argparse.Namespace) -> None:
    results, page, images = args.results, args.page, args.images
    files = sorted(results.glob("*.csv"))
    if not files:
        sys.exit(f"no results in {results}: run the benchmark first")
    images.mkdir(parents=True, exist_ok=True)
    sections = []
    for path in files:
        lines = read_lines(path)
        if not lines:
            continue
        name = lines[0]["machine"]
        labels = list(dict.fromkeys(line["label"] for line in lines))
        profiles = [profile for profile in PROFILES if any(line["profile"] == profile
                                                           for line in lines)]
        by_key = {(line["label"], line["profile"]): line for line in lines}
        figures = (("bandwidth", "bytes_in_per_s", "bytes received per second, per bot"),
                   ("rtt", "rtt_ms", "round trip time (ms)"))
        charts = []
        for figure, field, axis in figures:
            image = images / f"network-benchmark-{path.stem}-{figure}.svg"
            values = {key: float(line[field]) for key, line in by_key.items() if line[field]}
            image.write_text(bar_chart(f"{name}: {axis}", axis, values, labels, profiles))
            charts.append(f"![{name}: {axis}, by profile and run.](images/{image.name})")
        rows = [
            "| Profile | Run | In (B/s) | Out (B/s) | RTT (ms) | RTT jitter (ms) | Loss (%) | "
            "Snapshot jitter (ms) | Corrections |",
            "|---|---|---:|---:|---:|---:|---:|---:|---:|",
        ]
        for profile in profiles:
            first = by_key.get((labels[0], profile), {})
            for label in labels:
                line = by_key.get((label, profile))
                if line is None:
                    continue
                cells = [change(line[field], first.get(field, "")) for field in
                         ("bytes_in_per_s", "bytes_out_per_s", "rtt_ms", "rtt_jitter_ms",
                          "loss_percent", "snapshot_jitter_ms", "corrections")]
                rows.append(f"| {profile} | {label} | " + " | ".join(cells) + " |")
        first_line = lines[0]
        sections.append("\n".join([
            f"### {name}",
            "",
            f"{first_line['os']}; {first_line['bots']} bots for {first_line['seconds']} s per "
            f"profile; in parentheses, the change from {labels[0]}.",
            "",
            *rows,
            "",
            "\n\n".join(charts),
        ]))
    text = page.read_text()
    start, end = text.index(BEGIN) + len(BEGIN), text.index(END)
    page.write_text(text[:start] + "\n\n" + "\n\n".join(sections) + "\n\n" + text[end:])
    print(f"{page}: {len(sections)} machine(s)")


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
    command.add_argument("--label", default="baseline",
                         help="state of the code measured, e.g. delta-snapshots (baseline)")
    command.add_argument("--compress", type=int, metavar="BYTES",
                         help="have the server compress payloads from this size (off)")
    command.add_argument("--dev", default="lo", help="interface netem degrades (lo)")
    command.add_argument("--tcp-port", type=int, default=4342, help="server TCP port (4342)")
    command.add_argument("--udp-port", type=int, default=4343, help="server UDP port (4343)")
    command.add_argument("--output", type=Path, help="CSV to write instead of docs/...")
    command.add_argument("--dry-run", action="store_true",
                         help="print the netem commands instead of running them, and write "
                         "nothing unless --output is given")
    command.set_defaults(handler=run)
    report_command = commands.add_parser("report", help="rewrite the results page")
    report_command.add_argument("--results", type=Path, default=RESULTS, help=argparse.SUPPRESS)
    report_command.add_argument("--page", type=Path, default=PAGE, help=argparse.SUPPRESS)
    report_command.add_argument("--images", type=Path, default=IMAGES, help=argparse.SUPPRESS)
    report_command.set_defaults(handler=report)
    args = parser.parse_args()
    args.handler(args)


if __name__ == "__main__":
    main()
