#!/usr/bin/env bash
# Launcher-first local play (CipSoft path): Update/Play against a local tip.
# Unsets DEV Play bypass so the real package + assets install runs.
#
# Usage (via make desktop run):
#   bash scripts/lifecycle/run/play.sh
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
QTCLIENT="${TIBIA_CLIENT_DIR:-$(cd "$HERE/../../.." && pwd)}"
ROOT="$(cd "$QTCLIENT/../.." && pwd)"
TIP_DIR="${QTCLIENT_PLAY_TIP:-$ROOT/dist/play/tip}"

# Match product desktop install: DEV tip (macos-dev / linux-dev), not release.
if [[ -z "${DEV:-}" && -z "${TIBIA_PRESET:-}" && -z "${DEBUG:-}" ]]; then
  export DEV=1
fi

# shellcheck source=../lib/desktop-paths.sh
source "$HERE/../lib/desktop-paths.sh"
qtclient_desktop_paths "$QTCLIENT"

if [[ ! -x "$QTCLIENT_LAUNCHER_BIN" ]]; then
  echo "[play] missing $QTCLIENT_LAUNCHER_BIN — make qtclient launcher build" >&2
  exit 1
fi

if [[ ! -f "$TIP_DIR/tibiametadata.json" ]]; then
  echo "[play] no tip at $TIP_DIR — make desktop run emits it (or: bash scripts/lifecycle/package/play.sh)" >&2
  exit 1
fi

# Parity with CipSoft / demo-run: real tip install, not QTCLIENT_DEV_JSON autoplay.
unset QTCLIENT_DEV_JSON || true
unset QTCLIENT_AUTOPLAY || true
unset QTCLIENT_CLIENT_ARGS || true
unset QTCLIENT_MODS_DIR || true

export QTCLIENT_URL="${QTCLIENT_URL:-file://$TIP_DIR/tibiametadata.json}"
# Client revision must match the portal's tip (QTCAAC_ASSET_BASE). A pack
# brand's defaultCdn is its production CDN — without this, Play stamps the
# client with the live release revision and the portal refuses the login.
export QTCLIENT_CDN_URL="${QTCLIENT_CDN_URL:-http://127.0.0.1:9080}"
export QTCLIENT_ASSET_BASE="${QTCLIENT_ASSET_BASE:-$QTCLIENT_CDN_URL}"
echo "[play] tip $QTCLIENT_URL"
echo "[play] cdn $QTCLIENT_CDN_URL (release.json → QTCLIENT_CLIENT_REVISION)"
if [[ -f "$TIP_DIR/play-sources.json" ]]; then
  python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print("[play] packages:", ", ".join(p["directoryname"] for p in d.get("packages",[])))' \
    "$TIP_DIR/play-sources.json" 2>/dev/null || true
fi

echo "[play] starting $QTCLIENT_LAUNCHER_BIN (Update/Play → tip packages)"
cd "$QTCLIENT"

# Stock: CIP chrome + forward --no-mods to Play (demo-run parity).
# Product (make qtclient …): tip packages already stage mods/<id>; do not pass
# --no-mods or Play starts a stock CipSoft client (static.tibia.com urls).
#
# QTCLIENT_MODS_DIR here is launcher-only brand (pack/launcher/brand.json). The
# Play child strips it so the tip client loads mods/<id>/mod.json (loginBase /
# identity) next to the binary — see launcher host::startDetached.
launcher_args=()
if [[ "${PRODUCT:-0}" == "1" ]]; then
  pack="${QTCLIENT_PACK:-}"
  if [[ -z "${QTCLIENT_MODS_DIR:-}" && -n "$pack" ]]; then
    if [[ -f "$pack/launcher/brand.json" ]]; then
      export QTCLIENT_MODS_DIR="$pack/launcher"
    elif [[ -f "$pack/brand.json" ]]; then
      export QTCLIENT_MODS_DIR="$pack"
    fi
  fi
  if [[ -n "${QTCLIENT_MODS_DIR:-}" ]]; then
    echo "[play] product brand $QTCLIENT_MODS_DIR (launcher only; tip client uses package mods/)"
  else
    echo "[play] warning: PRODUCT=1 but no launcher brand.json — CIP AppData/chrome defaults" >&2
  fi
else
  launcher_args+=(--no-mods)
fi

# Client crashes never reach this shell (Play starts it detached): the watcher
# prints each new crash dump's stack here. `exec` keeps this pid, so $$ is the
# launcher's pid for the watcher.
if [[ -z "${CI:-}" && "${QTCLIENT_CRASH_WATCH:-1}" != "0" ]]; then
  bash "$HERE/../lib/watch-crashdumps.sh" "$$" &
fi

# shellcheck disable=SC2086
exec "$QTCLIENT_LAUNCHER_BIN" "${launcher_args[@]+"${launcher_args[@]}"}" ${ARGS:-} "$@"
