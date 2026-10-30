#!/usr/bin/env bash
# qtclient package — produce client-sdk or platform tip dirs for GHCR / local smoke.
#
# Env:
#   PACKAGE_KIND=sdk|desktop|wasm   (default: sdk)
#   PACKAGE_OUT=<dir>               destination (default under $RUNNER_TEMP or build/package)
#   PLATFORM=linux-x64|macos-arm64|windows-x64  (desktop)
#   BIN_DIR=<client bin dir>        (desktop; optional auto-detect)
#   WASM_BUILD=<tibia-wasm dir>     (wasm; default build/tibia-wasm)
#   QTCLIENT_HOST_BUILD_ID / SOURCE_COMMIT / GITHUB_SHA
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../.." && pwd)"
KIND="${PACKAGE_KIND:-sdk}"
OUT="${PACKAGE_OUT:-}"
HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-}}}"
if [[ -z "$HOST_ID" ]]; then
  HOST_ID="$(git -C "$OTC" rev-parse HEAD 2>/dev/null || echo unknown)"
fi
export QTCLIENT_HOST_BUILD_ID="$HOST_ID" SOURCE_COMMIT="$HOST_ID"

case "$KIND" in
  sdk)
    OUT="${OUT:-${RUNNER_TEMP:-$OTC/build/package}/client-sdk}"
    bash "$OTC/deploy/pack-client-sdk.sh" "$OUT"
    echo "PACKAGE_OUT=$OUT"
    ;;
  desktop)
    PLATFORM="${PLATFORM:?PLATFORM required for desktop package}"
    OUT="${OUT:-${RUNNER_TEMP:-$OTC/build/package}/client-$PLATFORM}"
    bin_dir="${BIN_DIR:-}"
    if [[ -z "$bin_dir" ]]; then
      if [[ -x "$OTC/deploy/find-client-bin.sh" ]]; then
        bin="$("$OTC/deploy/find-client-bin.sh" "$OTC")"
        bin_dir="$(dirname "$bin")"
      else
        echo "[qtclient] package desktop: set BIN_DIR" >&2
        exit 1
      fi
    fi
    bash "$OTC/deploy/pack-client-desktop.sh" \
      --platform "$PLATFORM" \
      --bin "$bin_dir" \
      --out "$OUT"
    echo "PACKAGE_OUT=$OUT"
    ;;
  wasm)
    WASM_BUILD="${WASM_BUILD:-$OTC/build/tibia-wasm}"
    OUT="${OUT:-${RUNNER_TEMP:-$OTC/build/package}/client-wasm}"
    bash "$OTC/deploy/pack-client-wasm.sh" "$WASM_BUILD" "$OUT"
    echo "PACKAGE_OUT=$OUT"
    ;;
  *)
    echo "[qtclient] package: unknown PACKAGE_KIND=$KIND (sdk|desktop|wasm)" >&2
    exit 1
    ;;
esac
