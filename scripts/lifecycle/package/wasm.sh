#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/../../.." && pwd)"
HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-}}}"
if [[ -z "$HOST_ID" ]]; then
  HOST_ID="$(git -C "$OTC" rev-parse HEAD 2>/dev/null || echo unknown)"
fi
export QTCLIENT_HOST_BUILD_ID="$HOST_ID" SOURCE_COMMIT="$HOST_ID"

# shellcheck source=_pack.sh
source "$HERE/_pack.sh"
WASM_SRC="${WASM_BUILD:-$OTC/build/tibia-wasm}"
OUT="${PACKAGE_OUT:-${RUNNER_TEMP:-$OTC/build/package}/client-wasm}"
pack_wasm "$WASM_SRC" "$OUT"
echo "PACKAGE_OUT=$OUT"
