#!/usr/bin/env bash
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../../.." && pwd)"
rm -rf "$OTC/build"/wasm* 2>/dev/null || true
echo "[qtclient] cleaned wasm"
