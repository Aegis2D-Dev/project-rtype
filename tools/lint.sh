#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
require() {
    command -v "$1" >/dev/null || {
        echo "$1 not found: pip install -r tools/requirements.txt (in the project .venv)" >&2
        exit 2
    }
}
require clang-tidy

build=${1:-build}
database="$build/compile_commands.json"
if [ ! -f "$database" ]; then
    echo "$database not found: configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON and build first" >&2
    exit 2
fi

files=$(python3 - "$database" <<'PY'
import json, os, subprocess, sys
compiled = {os.path.realpath(os.path.join(e["directory"], e["file"])) for e in json.load(open(sys.argv[1]))}
tracked = subprocess.check_output(["git", "ls-files", "*.cpp"], text=True).split()
print("\n".join(f for f in tracked if os.path.realpath(f) in compiled))
PY
)
if [ -z "$files" ]; then
    echo "no source of $database is tracked by git" >&2
    exit 2
fi

clang-tidy --version | sed -n 2p
extra=()
if [ "$(uname -s)" = Darwin ] && command -v xcrun >/dev/null; then
    extra+=("--extra-arg=-isysroot$(xcrun --show-sdk-path)")
fi
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
printf '%s\n' "$files" | xargs -P "$jobs" -n 1 clang-tidy -p "$build" --quiet ${extra[@]+"${extra[@]}"}
echo "clang-tidy: OK ($(printf '%s\n' "$files" | wc -l | tr -d ' ') files)"
