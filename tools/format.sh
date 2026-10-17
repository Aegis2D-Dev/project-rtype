#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
require() {
    command -v "$1" >/dev/null || {
        echo "$1 not found: pip install -r tools/requirements.txt (in the project .venv)" >&2
        exit 2
    }
}
require clang-format

case "${1:-}" in
    "") args=(-i) ;;
    --check) args=(--dry-run --Werror) ;;
    *) echo "usage: $0 [--check]" >&2; exit 2 ;;
esac

clang-format --version
git ls-files -z '*.cpp' '*.hpp' '*.h' | xargs -0 -r clang-format "${args[@]}"
echo "clang-format: OK"
