#!/usr/bin/env bash
# Launch the Qt/QML tibia app (cwd = apps/qtclient so data/things is found).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="${TIBIA_CLIENT_DIR:-$(cd "$HERE/.." && pwd)}"
FIND_BIN="$HERE/find-client-bin.sh"

BIN=""
LAUNCHER=""
# Prefer disk-QML dev builds (umbrella `make client-build`), then production/debug.
# Soft OUTPUT_NAME is `client` → client.app; product pack brands the .app (QtClient.app when qtclient is linked).
for preset in macos-dev linux-dev macos-release macos-debug linux-release linux-debug; do
  BIN="$("$FIND_BIN" "$OTC/build/$preset" 2>/dev/null || true)"
  LAUNCHER="$(find "$OTC/build/$preset" \( -path '*launcher.app/Contents/MacOS/launcher' -o -path '*/launcher/src/launcher' \) -type f 2>/dev/null | head -1 || true)"
  [[ -n "$BIN" ]] && break
done

if [[ -z "$BIN" ]]; then
  echo "[tibia] binary not found — run make client-build (umbrella, disk QML) or make -C apps/qtclient client-build (production)"
  exit 1
fi

echo "[tibia] starting $BIN (cwd $OTC)"
cd "$OTC"
# CipSoft Resources shape for local: Contents/Resources → data/{assets,sounds}
bash "$HERE/link-cip-resources.sh" "$BIN" "$OTC/data" || true
if [[ -n "$LAUNCHER" ]]; then
  export QTCLIENT_LAUNCHER_BIN="$LAUNCHER"
  echo "[tibia] Manage Clients → $LAUNCHER"
fi
exec "$BIN" "$@"
