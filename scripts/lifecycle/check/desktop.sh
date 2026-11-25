#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
QTCLIENT="$(cd "$HERE/../../.." && pwd)"
# shellcheck source=../lib/desktop-paths.sh
source "$QTCLIENT/scripts/lifecycle/lib/desktop-paths.sh"
qtclient_desktop_paths "$QTCLIENT"
if [[ ! -x "$QTCLIENT_CLIENT_BIN" ]]; then
  echo "[tibia] missing $QTCLIENT_CLIENT_BIN — make qtclient client build" >&2
  exit 1
fi
cd "$QTCLIENT"
exec "$QTCLIENT_CLIENT_BIN" --check-qml
