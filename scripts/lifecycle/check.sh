#!/usr/bin/env bash
# qtclient check — COMP=client|launcher|wasm
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/../.." && pwd)"
COMP="${COMP:-client}"
case "$COMP" in
  client)
    # shellcheck source=../deploy/desktop-paths.sh
    source "$OTC/scripts/deploy/desktop-paths.sh"
    qtclient_desktop_paths "$OTC"
    if [[ ! -x "$QTCLIENT_CLIENT_BIN" ]]; then
      echo "[tibia] missing $QTCLIENT_CLIENT_BIN — make qtclient client build" >&2
      exit 1
    fi
    cd "$OTC"
    exec "$QTCLIENT_CLIENT_BIN" --check-qml
    ;;
  launcher) echo "[qtclient] launcher check: make launcher build" ;;
  wasm) echo "[qtclient] wasm check: scripts/ci/check-wasm-play-boot.sh" ;;
  *) echo "[qtclient] check: unknown COMP=$COMP" >&2; exit 1 ;;
esac
