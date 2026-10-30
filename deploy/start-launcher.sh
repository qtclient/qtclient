#!/usr/bin/env bash
# Start the desktop client the way production does: the Qt CIP launcher spawns it.
# The binary itself stays `deploy/start-tibia.sh` for session-dump smoke and CI.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="${TIBIA_CLIENT_DIR:-$(cd "$HERE/.." && pwd)}"
ROOT="$(cd "$OTC/../.." && pwd)"
DEV_JSON="$ROOT/dist/cdn/latest/dev.json"

case "$(uname -s)" in
  Darwin) PRESET=macos-release ;;
  Linux) PRESET=linux-release ;;
  *) echo "[launcher] unsupported host: $(uname -s)" >&2; exit 1 ;;
esac

BIN="$(find "$OTC/build/$PRESET" \( -path '*launcher.app/Contents/MacOS/launcher' -o -path '*/launcher/src/launcher' \) -type f 2>/dev/null | head -1 || true)"
if [[ -z "$BIN" || ! -x "$BIN" ]]; then
  bash "$HERE/build-tibia.sh" --launcher
  BIN="$(find "$OTC/build/$PRESET" \( -path '*launcher.app/Contents/MacOS/launcher' -o -path '*/launcher/src/launcher' \) -type f 2>/dev/null | head -1 || true)"
fi
if [[ -z "$BIN" || ! -x "$BIN" ]]; then
  echo "[launcher] binary missing under $OTC/build/$PRESET" >&2
  exit 1
fi

export QTCLIENT_PORTAL_URL="${QTCLIENT_PORTAL_URL:-http://127.0.0.1:5173}"
export QTCLIENT_CDN_URL="${QTCLIENT_CDN_URL:-$QTCLIENT_PORTAL_URL/cdn}"
if [[ -f "$DEV_JSON" ]]; then
  export QTCLIENT_DEV_JSON="$DEV_JSON"
fi
if [[ "${QTCLIENT_AUTOPLAY:-}" == "1" && ! -f "$DEV_JSON" ]]; then
  echo "[launcher] no dist/cdn pointer — run make cdn once, then make client-build" >&2
  exit 1
fi

exec "$BIN"
