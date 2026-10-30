#!/usr/bin/env bash
# qtclient clean — COMP=client|launcher|wasm (or COMP=all)
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../.." && pwd)"
COMP="${COMP:-client}"
case "$COMP" in
  client)
    rm -rf "$OTC/build"/*-dev "$OTC/build"/macos "$OTC/build"/linux "$OTC/build"/windows 2>/dev/null || true
    echo "[qtclient] cleaned client build trees"
    ;;
  launcher)
    rm -rf "$OTC/build"/*/launcher 2>/dev/null || true
    echo "[qtclient] cleaned launcher"
    ;;
  wasm)
    rm -rf "$OTC/build"/wasm* 2>/dev/null || true
    echo "[qtclient] cleaned wasm"
    ;;
  all)
    COMP=client bash "$0"
    COMP=launcher bash "$0"
    COMP=wasm bash "$0"
    ;;
  *) echo "[qtclient] clean: unknown COMP=$COMP" >&2; exit 1 ;;
esac
