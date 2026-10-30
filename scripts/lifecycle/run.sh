#!/usr/bin/env bash
# qtclient run — COMP=client|launcher|wasm
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../.." && pwd)"
COMP="${COMP:-client}"
# shellcheck disable=SC2086
case "$COMP" in
  client) bash "$OTC/deploy/start-tibia.sh" --no-mods ${ARGS:-} ;;
  launcher) bash "$OTC/deploy/start-launcher.sh" ${ARGS:-} ;;
  wasm) echo "[qtclient] after wasm install: make -C ../.. portal run → /play" ;;
  *) echo "[qtclient] run: unknown COMP=$COMP" >&2; exit 1 ;;
esac
