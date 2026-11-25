#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
QTCLIENT="$(cd "$HERE/../../.." && pwd)"
HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-}}}"
if [[ -z "$HOST_ID" ]]; then
  HOST_ID="$(git -C "$QTCLIENT" rev-parse HEAD 2>/dev/null || echo unknown)"
fi
export QTCLIENT_HOST_BUILD_ID="$HOST_ID" SOURCE_COMMIT="$HOST_ID"

# shellcheck source=_pack.sh
source "$HERE/_pack.sh"
# COMP=wasm|browser → wasm SDK install (see _pack.sh pack_sdk).
OUT="${PACKAGE_OUT:-${RUNNER_TEMP:-$QTCLIENT/build/package}/client-sdk}"
pack_sdk "$OUT"
echo "PACKAGE_OUT=$OUT"
