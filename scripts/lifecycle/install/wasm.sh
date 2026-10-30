#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/../../.." && pwd)"
export DEV="${DEV:-}" DEBUG="${DEBUG:-}"
bash "$HERE/../build/wasm.sh"
python3 "$OTC/tools/publish/dev_cdn.py" --web
