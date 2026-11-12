#!/usr/bin/env bash
# Build desktop client + stage dist/cdn desktop tip.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/../../.." && pwd)"
export DEV="${DEV:-}" DEBUG="${DEBUG:-}"
bash "$HERE/../build/desktop.sh"
python3 "$OTC/tools/publish/dev_cdn.py" --desktop
