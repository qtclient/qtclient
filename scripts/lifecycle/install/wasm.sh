#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
QTCLIENT="$(cd "$HERE/../../.." && pwd)"
export DEV="${DEV:-}" DEBUG="${DEBUG:-}"
bash "$HERE/../build/wasm.sh"
python3 "$QTCLIENT/tools/publish/dev_cdn.py" --web
