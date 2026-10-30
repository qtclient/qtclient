#!/usr/bin/env bash
# qtclient package — produce client-sdk or platform tip dirs for GHCR / local smoke.
#
# Env:
#   PACKAGE_KIND=sdk|desktop|wasm   (default: sdk)
#   PACKAGE_OUT=<dir>
#   PLATFORM=linux-x64|macos-arm64|windows-x64  (desktop)
#   BIN_DIR=<client bin dir>        (desktop; optional auto-detect)
#   WASM_BUILD=<tibia-wasm dir>     (wasm; default build/tibia-wasm)
#   QTCLIENT_HOST_BUILD_ID / SOURCE_COMMIT / GITHUB_SHA
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/../.." && pwd)"
KIND="${PACKAGE_KIND:-sdk}"
OUT="${PACKAGE_OUT:-}"
HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-}}}"
if [[ -z "$HOST_ID" ]]; then
  HOST_ID="$(git -C "$OTC" rev-parse HEAD 2>/dev/null || echo unknown)"
fi
export QTCLIENT_HOST_BUILD_ID="$HOST_ID" SOURCE_COMMIT="$HOST_ID"

pack_sdk() {
  local DEST="$1"
  rm -rf "$DEST"
  mkdir -p "$DEST/include/qtclient" "$DEST/cmake" "$DEST/client/tibia" "$DEST/cipgen"

  cp -a "$OTC/include/qtclient/." "$DEST/include/qtclient/"
  rsync -a --include '*/' --include '*.h' --include '*.hpp' --exclude '*' \
    "$OTC/client/tibia/" "$DEST/client/tibia/"
  mkdir -p "$DEST/include"
  rsync -a --include '*/' --include '*.h' --include '*.hpp' --exclude '*' \
    "$OTC/client/tibia/" "$DEST/include/"

  cp -a "$OTC/cmake/QtClientMod.cmake" "$DEST/cmake/"
  cp -a "$OTC/cmake/QtClientHost.cmake" "$DEST/cmake/"
  cp -a "$OTC/cmake/GeneratePackQmldir.cmake" "$DEST/cmake/"
  cp -a "$OTC/cmake/QtClientConfig.cmake.in" "$DEST/cmake/" 2>/dev/null || true

  python3 "$OTC/tools/cip/gen_cpp.py" \
    "$OTC/client/upstream/contract" \
    "$DEST/cipgen" \
    --chunks 12 \
    --qml "$OTC/client/upstream/qml/qmlcomponents"
  test -f "$DEST/cipgen/cip_gen.h"

  cat >"$DEST/cmake/QtClientConfig.cmake" <<EOF
# qtclient client-sdk (hostBuildId=${HOST_ID})
set(QTCLIENT_ROOT "\${CMAKE_CURRENT_LIST_DIR}/..")
set(QTCLIENT_HOST_BUILD_ID "${HOST_ID}")
set(TIBIA_ROOT "\${QTCLIENT_ROOT}" CACHE PATH "qtclient SDK root" FORCE)
list(APPEND CMAKE_MODULE_PATH "\${QTCLIENT_ROOT}/cmake")
include("\${QTCLIENT_ROOT}/cmake/QtClientMod.cmake")
if(NOT TARGET QtClient::Host)
  add_library(QtClient::Host INTERFACE IMPORTED)
  set_target_properties(QtClient::Host PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "\${QTCLIENT_ROOT}/include;\${QTCLIENT_ROOT}/include/engine;\${QTCLIENT_ROOT}/cipgen;\${QTCLIENT_ROOT}/client/tibia;\${QTCLIENT_ROOT}/client/tibia/engine")
endif()
set(QtClient_FOUND TRUE)
EOF

  cat >"$DEST/client-sdk.json" <<EOF
{
  "hostBuildId": "${HOST_ID}",
  "schema": 1,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
  cp -f "$DEST/client-sdk.json" "$DEST/host-sdk.json"
  echo "[package] sdk hostBuildId=${HOST_ID} → ${DEST}"
}

pack_desktop() {
  local PLATFORM="$1" BIN="$2" DEST="$3"
  export QTCLIENT_APP_NAME="${QTCLIENT_APP_NAME:-qtclient}"
  export QTCLIENT_BUNDLE_ID="${QTCLIENT_BUNDLE_ID:-com.qtclient.client}"
  export QTCLIENT_STORE_META="${QTCLIENT_STORE_META:-.qtclient}"

  rm -rf "$DEST"
  mkdir -p "$DEST/bin" "$DEST/engine"

  python3 "$OTC/tools/publish/package_desktop.py" \
    --platform "$PLATFORM" \
    --bin "$BIN" \
    --engine-bundle "$DEST/engine"

  for name in client launcher; do
    if [[ -f "$BIN/$name" ]]; then
      cp -a "$BIN/$name" "$DEST/bin/$name"
    elif [[ -f "$BIN/$name.exe" ]]; then
      cp -a "$BIN/$name.exe" "$DEST/bin/$name.exe"
    elif [[ -d "$BIN/$name.app" ]]; then
      cp -a "$BIN/$name.app" "$DEST/bin/$name.app"
    fi
  done

  cat >"$DEST/host.json" <<EOF
{
  "hostBuildId": "${HOST_ID}",
  "platform": "${PLATFORM}",
  "schema": 1,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
  echo "[package] desktop ${PLATFORM} hostBuildId=${HOST_ID} → ${DEST}"
}

pack_wasm() {
  local SRC="$1" DEST="$2"
  export TIBIA_ENGINE_SHA="$HOST_ID"
  rm -rf "$DEST"
  mkdir -p "$DEST"
  bash "$OTC/scripts/deploy/pack-tibia-wasm.sh" "$SRC" "$DEST"
  cat >"$DEST/host.json" <<EOF
{
  "hostBuildId": "${HOST_ID}",
  "platform": "wasm",
  "schema": 1,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
  echo "[package] wasm hostBuildId=${HOST_ID} → ${DEST}"
}

case "$KIND" in
  sdk)
    OUT="${OUT:-${RUNNER_TEMP:-$OTC/build/package}/client-sdk}"
    pack_sdk "$OUT"
    ;;
  desktop)
    PLATFORM="${PLATFORM:?PLATFORM required for desktop package}"
    OUT="${OUT:-${RUNNER_TEMP:-$OTC/build/package}/client-$PLATFORM}"
    bin_dir="${BIN_DIR:-}"
    if [[ -z "$bin_dir" ]]; then
      bin="$("$OTC/scripts/deploy/find-client-bin.sh" "$OTC")"
      bin_dir="$(dirname "$bin")"
    fi
    pack_desktop "$PLATFORM" "$bin_dir" "$OUT"
    ;;
  wasm)
    WASM_SRC="${WASM_BUILD:-$OTC/build/tibia-wasm}"
    OUT="${OUT:-${RUNNER_TEMP:-$OTC/build/package}/client-wasm}"
    pack_wasm "$WASM_SRC" "$OUT"
    ;;
  *)
    echo "[qtclient] package: unknown PACKAGE_KIND=$KIND (sdk|desktop|wasm)" >&2
    exit 1
    ;;
esac
echo "PACKAGE_OUT=$OUT"
