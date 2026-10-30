#!/usr/bin/env bash
# qtclient check — COMP=client|launcher|wasm
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../.." && pwd)"
COMP="${COMP:-client}"
case "$COMP" in
  client) bash "$OTC/deploy/start-tibia.sh" --check-qml ;;
  launcher) echo "[qtclient] launcher check: make launcher build" ;;
  wasm) echo "[qtclient] wasm check: scripts/ci/check-wasm-play-boot.sh" ;;
  *) echo "[qtclient] check: unknown COMP=$COMP" >&2; exit 1 ;;
esac
