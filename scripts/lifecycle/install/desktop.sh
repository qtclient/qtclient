#!/usr/bin/env bash
# Build desktop client + stage dist/cdn desktop tip.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
QTCLIENT="$(cd "$HERE/../../.." && pwd)"
export DEV="${DEV:-}" DEBUG="${DEBUG:-}"
bash "$HERE/../build/desktop.sh"
python3 "$QTCLIENT/tools/publish/dev_cdn.py" --desktop
