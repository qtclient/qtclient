#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="${TIBIA_CLIENT_DIR:-$(cd "$HERE/../../.." && pwd)}"
# shellcheck source=../lib/desktop-paths.sh
source "$OTC/scripts/lifecycle/lib/desktop-paths.sh"
qtclient_desktop_paths "$OTC"

echo "[qtclient] after wasm install: make -C ../.. portal run → /play"
