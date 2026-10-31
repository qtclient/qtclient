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
# Prefer QTCLIENT_PLATFORM: on Windows, MSVC's Platform (case-insensitive)
# collides with env PLATFORM and Actions drops the step value.
PLATFORM="${QTCLIENT_PLATFORM:-${PLATFORM:-}}"
PLATFORM="${PLATFORM:?QTCLIENT_PLATFORM (or PLATFORM) required for desktop package}"
OUT="${PACKAGE_OUT:-${RUNNER_TEMP:-$OTC/build/package}/client-$PLATFORM}"
bin_dir="${BIN_DIR:-}"
if [[ -z "$bin_dir" ]]; then
  bin="$("$OTC/scripts/lifecycle/lib/find-client-bin.sh" "$OTC")"
  bin_dir="$(dirname "$bin")"
fi
pack_desktop "$PLATFORM" "$bin_dir" "$OUT"
echo "PACKAGE_OUT=$OUT"
