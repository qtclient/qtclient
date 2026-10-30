#!/usr/bin/env bash
# Stage stock MAIN_MODULE wasm tip (no product pack) for GHCR / Release assemble.
# Usage: pack-client-wasm.sh <build-tibia-wasm-dir> <dest-dir>
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/.." && pwd)"
SRC="${1:?build dir with tibia.js/wasm}"
DEST="${2:?dest dir}"
HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-$(git -C "$OTC" rev-parse HEAD)}}}"
export TIBIA_ENGINE_SHA="$HOST_ID"
export QTCLIENT_HOST_BUILD_ID="$HOST_ID"

rm -rf "$DEST"
mkdir -p "$DEST"
bash "$OTC/deploy/pack-tibia-wasm.sh" "$SRC" "$DEST"
cat >"$DEST/host.json" <<EOF
{
  "hostBuildId": "${HOST_ID}",
  "platform": "wasm",
  "schema": 1,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
echo "[pack-client-wasm] hostBuildId=${HOST_ID} → ${DEST}"
