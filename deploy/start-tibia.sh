#!/usr/bin/env bash
# Launch the qtclient tibia app (cwd = apps/qtclient so data/ is found).
# Binary: build/<preset>/bin/client[.app]/…] — see desktop-paths.sh
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="${TIBIA_CLIENT_DIR:-$(cd "$HERE/.." && pwd)}"
# shellcheck source=desktop-paths.sh
source "$HERE/desktop-paths.sh"
qtclient_desktop_paths "$OTC"

if [[ ! -x "$QTCLIENT_CLIENT_BIN" ]]; then
  echo "[tibia] missing $QTCLIENT_CLIENT_BIN — make qtclient client build (preset=$QTCLIENT_PRESET)" >&2
  exit 1
fi

LAUNCHER=""
if [[ -x "$QTCLIENT_LAUNCHER_BIN" ]]; then
  LAUNCHER="$QTCLIENT_LAUNCHER_BIN"
fi

# Login URLs: single tip pack / QTCLIENT_MODS_DIR / --default-mod, else real CipSoft.
echo "[tibia] starting $QTCLIENT_CLIENT_BIN (cwd $OTC)"
cd "$OTC"
bash "$HERE/link-cip-resources.sh" "$QTCLIENT_CLIENT_BIN" "$OTC/data" || true
if [[ -n "$LAUNCHER" ]]; then
  export QTCLIENT_LAUNCHER_BIN="$LAUNCHER"
  echo "[tibia] Manage Clients → $LAUNCHER"
fi
exec "$QTCLIENT_CLIENT_BIN" "$@"
