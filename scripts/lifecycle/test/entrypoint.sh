#!/usr/bin/env bash
# qtclient test — COMP=client|launcher|wasm
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../../.." && pwd)"
COMP="${COMP:-client}"
case "$COMP" in
  client) bash "$OTC/scripts/ci/check-session-dump-smoke.sh" ;;
  launcher|wasm) echo "[qtclient] $COMP test: n/a" ;;
  *) echo "[qtclient] test: unknown COMP=$COMP" >&2; exit 1 ;;
esac
